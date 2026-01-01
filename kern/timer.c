#include <inc/assert.h>
#include <inc/memlayout.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/types.h>
#include <inc/uefi.h>
#include <inc/x86.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/pmap.h>
#include <kern/pmap.h>
#include <kern/timer.h>
#include <kern/trap.h>
#include <kern/tsc.h>

#define kilo      (1000ULL)
#define Mega      (kilo * kilo)
#define Giga      (kilo * Mega)
#define Tera      (kilo * Giga)
#define Peta      (kilo * Tera)
#define ULONG_MAX ~0UL

#if LAB <= 6
/* Early variant of memory mapping that does 1:1 aligned area mapping
 * in 2MB pages. You will need to reimplement this code with proper
 * virtual memory mapping in the future. */
void *
mmio_map_region(physaddr_t pa, size_t size) {
    void map_addr_early_boot(uintptr_t addr, uintptr_t addr_phys, size_t sz);
    const physaddr_t base_2mb = 0x200000;
    uintptr_t org = pa;
    size += pa & (base_2mb - 1);
    size += (base_2mb - 1);
    pa &= ~(base_2mb - 1);
    size &= ~(base_2mb - 1);
    map_addr_early_boot(pa, pa, size);
    return (void *)org;
}
void *
mmio_remap_last_region(physaddr_t pa, void *addr, size_t oldsz, size_t newsz) {
    return mmio_map_region(pa, newsz);
}
#endif

struct Timer timertab[MAX_TIMERS];
struct Timer *timer_for_schedule;

struct Timer timer_hpet0 = {
        .timer_name = "hpet0",
        .timer_init = hpet_init,
        .get_cpu_freq = hpet_cpu_frequency,
        .enable_interrupts = hpet_enable_interrupts_tim0,
        .handle_interrupts = hpet_handle_interrupts_tim0,
};

struct Timer timer_hpet1 = {
        .timer_name = "hpet1",
        .timer_init = hpet_init,
        .get_cpu_freq = hpet_cpu_frequency,
        .enable_interrupts = hpet_enable_interrupts_tim1,
        .handle_interrupts = hpet_handle_interrupts_tim1,
};

struct Timer timer_acpipm = {
        .timer_name = "pm",
        .timer_init = acpi_enable,
        .get_cpu_freq = pmtimer_cpu_frequency,
};

void
acpi_enable(void) {
    FADT *fadt = get_fadt();
    outb(fadt->SMI_CommandPort, fadt->AcpiEnable);
    while ((inw(fadt->PM1aControlBlock) & 1) == 0) /* nothing */
        ;
}

static bool
acpi_checksum_ok(const void *addr, size_t len) {
    const uint8_t *p = (const uint8_t *)addr;
    uint8_t s = 0;
    for (size_t i = 0; i < len; i++)
        s = (uint8_t)(s + p[i]);
    return s == 0;
}

static ACPISDTHeader *
map_sdt_full(physaddr_t pa) {
    ACPISDTHeader *h = (ACPISDTHeader *)mmio_map_region(pa, sizeof(ACPISDTHeader));
    if (!h)
        return NULL;

    uint32_t len = h->Length;
    if (len < sizeof(ACPISDTHeader))
        return NULL;

    h = (ACPISDTHeader *)mmio_remap_last_region(pa, h, sizeof(ACPISDTHeader), len);
    if (!h)
        return NULL;

    if (!acpi_checksum_ok(h, len))
        return NULL;

    return h;
}

static void *
acpi_find_table(const char *sign) {
    // 1) Map RSDP
    physaddr_t rsdp_pa = (physaddr_t)uefi_lp->ACPIRoot;
    RSDP *rsdp = (RSDP *)mmio_map_region(rsdp_pa, sizeof(RSDP));
    if (!rsdp)
        return NULL;

    if (memcmp(rsdp->Signature, "RSD PTR ", 8) != 0)
        return NULL;

    // RSDP v1 checksum: first 20 bytes
    if (!acpi_checksum_ok(rsdp, 20))
        return NULL;

    // 2) Prefer XSDT if available (Revision >= 2)
    if (rsdp->Revision >= 2 && rsdp->XsdtAddress) {
        physaddr_t xsdt_pa = (physaddr_t)rsdp->XsdtAddress;
        ACPISDTHeader *xsdt = map_sdt_full(xsdt_pa);
        if (xsdt && memcmp(xsdt->Signature, "XSDT", 4) == 0) {
            uint32_t len = xsdt->Length;
            if (len >= sizeof(ACPISDTHeader)) {
                size_t n = (len - sizeof(ACPISDTHeader)) / 8;
                const uint8_t *ents = (const uint8_t *)xsdt + sizeof(ACPISDTHeader);

                for (size_t i = 0; i < n; i++) {
                    uint64_t entry;
                    memcpy(&entry, ents + i * 8, sizeof(entry));   
                    physaddr_t pa = (physaddr_t)entry;

                    ACPISDTHeader *sdt = map_sdt_full(pa);
                    if (!sdt)
                        continue;

                    if (memcmp(sdt->Signature, sign, 4) == 0)
                        return sdt;
                }
            }
        }
    }

    // 3) Fallback to RSDT (32-bit entries)
    if (!rsdp->RsdtAddress)
        return NULL;

    physaddr_t rsdt_pa = (physaddr_t)rsdp->RsdtAddress;
    ACPISDTHeader *rsdt = map_sdt_full(rsdt_pa);
    if (!rsdt || memcmp(rsdt->Signature, "RSDT", 4) != 0)
        return NULL;

    uint32_t len = rsdt->Length;
    size_t n = (len - sizeof(ACPISDTHeader)) / 4;
    const uint8_t *ents = (const uint8_t *)rsdt + sizeof(ACPISDTHeader);

    for (size_t i = 0; i < n; i++) {
        uint32_t entry32;
        memcpy(&entry32, ents + i * 4, sizeof(entry32));        // safe even if packed
        physaddr_t pa = (physaddr_t)entry32;

        ACPISDTHeader *sdt = map_sdt_full(pa);
        if (!sdt)
            continue;

        if (memcmp(sdt->Signature, sign, 4) == 0)
            return sdt;
    }

    return NULL;
}

MCFG *
get_mcfg(void) {
    static MCFG *kmcfg;
    if (!kmcfg) {
        struct AddressSpace *as = switch_address_space(&kspace);
        kmcfg = acpi_find_table("MCFG");
        switch_address_space(as);
    }

    return kmcfg;
}

#define MAX_SEGMENTS 16

uintptr_t
make_fs_args(char *ustack_top) {

    MCFG *mcfg = get_mcfg();
    if (!mcfg) {
        cprintf("MCFG table is absent!");
        return (uintptr_t)ustack_top;
    }

    char *argv[MAX_SEGMENTS + 3] = {0};

    /* Store argv strings on stack */

    ustack_top -= 3;
    argv[0] = ustack_top;
    nosan_memcpy(argv[0], "fs", 3);

    int nent = (mcfg->h.Length - sizeof(MCFG)) / sizeof(CSBAA);
    if (nent > MAX_SEGMENTS)
        nent = MAX_SEGMENTS;

    for (int i = 0; i < nent; i++) {
        CSBAA *ent = &mcfg->Data[i];

        char arg[64];
        snprintf(arg, sizeof(arg) - 1, "ecam=%llx:%04x:%02x:%02x",
                 (long long)ent->BaseAddress, ent->SegmentGroup, ent->StartBus, ent->EndBus);

        int len = strlen(arg) + 1;
        ustack_top -= len;
        nosan_memcpy(ustack_top, arg, len);
        argv[i + 1] = ustack_top;
    }

    char arg[64];
    snprintf(arg, sizeof(arg) - 1, "tscfreq=%llx", (long long)tsc_calibrate());
    int len = strlen(arg) + 1;
    ustack_top -= len;
    nosan_memcpy(ustack_top, arg, len);
    argv[nent + 1] = ustack_top;

    /* Realign stack */
    ustack_top = (char *)((uintptr_t)ustack_top & ~(2 * sizeof(void *) - 1));

    /* Copy argv vector */
    ustack_top -= (nent + 3) * sizeof(void *);
    nosan_memcpy(ustack_top, argv, (nent + 3) * sizeof(argv[0]));

    char **argv_arg = (char **)ustack_top;
    long argc_arg = nent + 2;

    /* Store argv and argc arguemnts on stack */
    ustack_top -= sizeof(void *);
    nosan_memcpy(ustack_top, &argv_arg, sizeof(argv_arg));
    ustack_top -= sizeof(void *);
    nosan_memcpy(ustack_top, &argc_arg, sizeof(argc_arg));

    /* and return new stack pointer */
    return (uintptr_t)ustack_top;
}

/* Obtain and map FADT ACPI table address. */
FADT *
get_fadt(void) {
    // LAB 5: Your code here
    // (use acpi_find_table)
    // HINT: ACPI table signatures are
    //       not always as their names
    return acpi_find_table("FACP");
}

/* Obtain and map RSDP ACPI table address. */
HPET *
get_hpet(void) {
    // LAB 5: Your code here
    // (use acpi_find_table)
    return acpi_find_table("HPET");
}

/* Getting physical HPET timer address from its table. */
HPETRegister *
hpet_register(void) {
    HPET *hpet_timer = get_hpet();
    if (!hpet_timer->address.address) panic("hpet is unavailable\n");

    uintptr_t paddr = hpet_timer->address.address;
    return mmio_map_region(paddr, sizeof(HPETRegister));
}

/* Debug HPET timer state. */
void
hpet_print_struct(void) {
    HPET *hpet = get_hpet();
    assert(hpet != NULL);
    cprintf("signature = %s\n", (hpet->h).Signature);
    cprintf("length = %08x\n", (hpet->h).Length);
    cprintf("revision = %08x\n", (hpet->h).Revision);
    cprintf("checksum = %08x\n", (hpet->h).Checksum);

    cprintf("oem_revision = %08x\n", (hpet->h).OEMRevision);
    cprintf("creator_id = %08x\n", (hpet->h).CreatorID);
    cprintf("creator_revision = %08x\n", (hpet->h).CreatorRevision);

    cprintf("hardware_rev_id = %08x\n", hpet->hardware_rev_id);
    cprintf("comparator_count = %08x\n", hpet->comparator_count);
    cprintf("counter_size = %08x\n", hpet->counter_size);
    cprintf("reserved = %08x\n", hpet->reserved);
    cprintf("legacy_replacement = %08x\n", hpet->legacy_replacement);
    cprintf("pci_vendor_id = %08x\n", hpet->pci_vendor_id);
    cprintf("hpet_number = %08x\n", hpet->hpet_number);
    cprintf("minimum_tick = %08x\n", hpet->minimum_tick);

    cprintf("address_structure:\n");
    cprintf("address_space_id = %08x\n", (hpet->address).address_space_id);
    cprintf("register_bit_width = %08x\n", (hpet->address).register_bit_width);
    cprintf("register_bit_offset = %08x\n", (hpet->address).register_bit_offset);
    cprintf("address = %08lx\n", (unsigned long)(hpet->address).address);
}

static volatile HPETRegister *hpetReg;
/* HPET timer period (in femtoseconds) */
static uint64_t hpetFemto = 0;
/* HPET timer frequency */
static uint64_t hpetFreq = 0;

/* HPET timer initialisation */
void
hpet_init() {
    if (hpetReg == NULL) {
        nmi_disable();
        hpetReg = hpet_register();
        uint64_t cap = hpetReg->GCAP_ID;
        hpetFemto = (uintptr_t)(cap >> 32);
        if (!(cap & HPET_LEG_RT_CAP)) panic("HPET has no LegacyReplacement mode");

        // cprintf("hpetFemto = %llu\n", hpetFemto);
        hpetFreq = (1 * Peta) / hpetFemto;
        // cprintf("HPET: Frequency = %d.%03dMHz\n", (uintptr_t)(hpetFreq / Mega), (uintptr_t)(hpetFreq % Mega));
        /* Enable ENABLE_CNF bit to enable timer */
        hpetReg->GEN_CONF |= HPET_ENABLE_CNF;
        nmi_enable();
    }
}

/* HPET register contents debugging. */
void
hpet_print_reg(void) {
    cprintf("GCAP_ID = %016lx\n", (unsigned long)hpetReg->GCAP_ID);
    cprintf("GEN_CONF = %016lx\n", (unsigned long)hpetReg->GEN_CONF);
    cprintf("GINTR_STA = %016lx\n", (unsigned long)hpetReg->GINTR_STA);
    cprintf("MAIN_CNT = %016lx\n", (unsigned long)hpetReg->MAIN_CNT);
    cprintf("TIM0_CONF = %016lx\n", (unsigned long)hpetReg->TIM0_CONF);
    cprintf("TIM0_COMP = %016lx\n", (unsigned long)hpetReg->TIM0_COMP);
    cprintf("TIM0_FSB = %016lx\n", (unsigned long)hpetReg->TIM0_FSB);
    cprintf("TIM1_CONF = %016lx\n", (unsigned long)hpetReg->TIM1_CONF);
    cprintf("TIM1_COMP = %016lx\n", (unsigned long)hpetReg->TIM1_COMP);
    cprintf("TIM1_FSB = %016lx\n", (unsigned long)hpetReg->TIM1_FSB);
    cprintf("TIM2_CONF = %016lx\n", (unsigned long)hpetReg->TIM2_CONF);
    cprintf("TIM2_COMP = %016lx\n", (unsigned long)hpetReg->TIM2_COMP);
    cprintf("TIM2_FSB = %016lx\n", (unsigned long)hpetReg->TIM2_FSB);
}

/* HPET main timer counter value. */
uint64_t
hpet_get_main_cnt(void) {
    return hpetReg->MAIN_CNT;
}


static inline uint64_t hpet_ticks_fs128(__uint128_t fs){
    return (uint64_t)(fs / (__uint128_t)hpetFemto);
}

/* - Configure HPET timer 0 to trigger every 0.5 seconds on IRQ_TIMER line
 * - Configure HPET timer 1 to trigger every 1.5 seconds on IRQ_CLOCK line
 *
 * HINT To be able to use HPET as PIT replacement consult
 *      LegacyReplacement functionality in HPET spec.
 * HINT Don't forget to unmask interrupt in PIC */
void
hpet_enable_interrupts_tim0(void) {
    // LAB 5: Your code here
    uint64_t ticks = hpet_ticks_fs128((__uint128_t)Peta / 2);
    hpetReg->GINTR_STA = ~0ULL;
    uint64_t c = hpetReg->TIM0_CONF;
    if (c & HPET_TN_PER_INT_CAP) c |= HPET_TN_TYPE_CNF;
    c |= HPET_TN_VAL_SET_CNF | HPET_TN_INT_ENB_CNF;
    hpetReg->TIM0_CONF = c;
    hpetReg->TIM0_COMP = ticks;
    hpetReg->GEN_CONF |= HPET_LEG_RT_CNF | HPET_ENABLE_CNF;
    pic_irq_unmask(IRQ_TIMER);
}

void
hpet_enable_interrupts_tim1(void) {
    // LAB 5: Your code here
    uint64_t ticks = hpet_ticks_fs128((__uint128_t)Peta * 3 / 2);
    hpetReg->GINTR_STA = ~0ULL;
    uint64_t c = hpetReg->TIM1_CONF;
    if (c & HPET_TN_PER_INT_CAP) c |= HPET_TN_TYPE_CNF;
    c |= HPET_TN_VAL_SET_CNF | HPET_TN_INT_ENB_CNF;
    hpetReg->TIM1_CONF = c;
    hpetReg->TIM1_COMP = ticks;
    hpetReg->GEN_CONF |= HPET_LEG_RT_CNF | HPET_ENABLE_CNF;
    pic_irq_unmask(IRQ_CLOCK);
}

void
hpet_handle_interrupts_tim0(void) {
    pic_send_eoi(IRQ_TIMER);
}

void
hpet_handle_interrupts_tim1(void) {
    pic_send_eoi(IRQ_CLOCK);
}

/* Calculate CPU frequency in Hz with the help with HPET timer.
 * HINT Use hpet_get_main_cnt function and do not forget about
 * about pause instruction. */
uint64_t
hpet_cpu_frequency(void) {
    static uint64_t cpu_freq;

    // LAB 5: Your code here
    
    uint64_t tsc1, tsc2;
    uint64_t hpet_start, hpet_end;
    uint64_t next_moment = hpetFreq / 10;  // 100ms delta

    hpet_end = hpet_get_main_cnt() + next_moment;
    tsc1 = read_tsc();

    while (hpet_get_main_cnt() < hpet_end) {
        asm("pause");
    }

    hpet_start = hpet_end - next_moment;
    hpet_end = hpet_get_main_cnt();
    tsc2 = read_tsc();

    cpu_freq = tsc2 - tsc1;
    cpu_freq *= hpetFreq;
    cpu_freq /= (hpet_end - hpet_start);

    return cpu_freq;
}


uint32_t
pmtimer_get_timeval(void) {
    FADT *fadt = get_fadt();
    return inl(fadt->PMTimerBlock);
}

/* Calculate CPU frequency in Hz with the help with ACPI PowerManagement timer.
 * HINT Use pmtimer_get_timeval function and do not forget that ACPI PM timer
 *      can be 24-bit or 32-bit. */
uint64_t
pmtimer_cpu_frequency(void) {
    static uint64_t cpu_freq;

    // LAB 5: Your code here

    uint32_t TMR_VAL_EXT = 1U << 8;
    uint32_t fl24 = (get_fadt()->Flags & TMR_VAL_EXT) >> 8;
    
    uint64_t tsc_ticks = 0;
    uint64_t pmt_ticks = 0;
    uint64_t pmt_start = pmtimer_get_timeval();
    uint64_t tsc_start = read_tsc();
    uint64_t next_moment = PM_FREQ / 10 + pmt_start;    // moment after 100ms delta
    pmt_ticks = pmt_start;

    while (pmt_start < next_moment) {
        uint64_t pmt_cur = pmtimer_get_timeval();
        tsc_ticks = read_tsc();

        if (pmt_cur > pmt_ticks) {
            pmt_start += pmt_cur - pmt_ticks;
        } else {
            if (fl24) {
                pmt_start += pmt_cur + 0xFFFFFFFF - pmt_ticks;  // 32-bit overflow
            } else {
                pmt_start += pmt_cur + 0x00FFFFFF - pmt_ticks;  // 24-bit overflow
            }
        }

        pmt_ticks = pmt_cur;
    }

    cpu_freq = (tsc_ticks - tsc_start) * 10;
    return cpu_freq;
}