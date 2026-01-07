## Обзор и подход
- Добавим поддержку таймаутов в миллисекундах для блокирующего приема (sys_ipc_recv) и новой блокирующей отправки (sys_ipc_send). Сохраним текущий неблокирующий sys_ipc_try_send для совместимости.
- Используем существующую модель блокировки процессов: перевод ENV в ENV_NOT_RUNNABLE и пробуждение через планировщик. Без busy waiting: ожидание реализуется через таймерные прерывания и проверку дедлайнов.
- Для возврата результата из «пробужденного» системного вызова установим код возврата в Trapframe (tf->tf_regs.reg_rax) перед переводом ENV в ENV_RUNNABLE.

## Изменения в интерфейсах
- Расширим сигнатуру sys_ipc_recv: добавить timeout_ms (uint32_t). Диспетчер будет вызывать новую реализацию с тремя аргументами.
  - Обновить объявления и обертку в пользовательской библиотеке: [lib/syscall.c](file:///c:/Users/rusla/OneDrive/%D0%A0%D0%B0%D0%B1%D0%BE%D1%87%D0%B8%D0%B9%20%D1%81%D1%82%D0%BE%D0%BB/OS-Course-2025/lib/syscall.c#L141-L151), [inc/lib.h](file:///c:/Users/rusla/OneDrive/%D0%A0%D0%B0%D0%B1%D0%BE%D1%87%D0%B8%D0%B9%20%D1%81%D1%82%D0%BE%D0%BB/OS-Course-2025/inc/lib.h#L95-L101).
- Добавим новый системный вызов sys_ipc_send(envid_t, value, srcva, size, perm, timeout_ms) — блокирующая отправка с таймаутом.
  - Присвоим номеру SYS_ipc_send в [inc/syscall.h](file:///c:/Users/rusla/OneDrive/%D0%A0%D0%B0%D0%B1%D0%BE%D1%87%D0%B8%D0%B9%20%D1%81%D1%82%D0%BE%D0%BB/OS-Course-2025/inc/syscall.h#L4-L24) и добавим разбор в [kern/syscall.c](file:///c:/Users/rusla/OneDrive/%D0%A0%D0%B0%D0%B1%D0%BE%D1%87%D0%B8%D0%B9%20%D1%81%D1%82%D0%BE%D0%BB/OS-Course-2025/kern/syscall.c#L491-L530).
- Ошибки: введем E_TIMEDOUT в [inc/error.h](file:///c:/Users/rusla/OneDrive/%D0%A0%D0%B0%D0%B1%D0%BE%D1%87%D0%B8%D0%B9%20%D1%81%D1%82%D0%BE%D0%BB/OS-Course-2025/inc/error.h#L6-L31) и добавим строку в [lib/printfmt.c](file:///c:/Users/rusla/OneDrive/%D0%A0%D0%B0%D0%B1%D0%BE%D1%87%D0%B8%D0%B9%20%D1%81%D1%82%D0%BE%D0%BB/OS-Course-2025/lib/printfmt.c#L21-L41) (синхронизация MAXERROR).

## Структуры ядра
- Расширим struct Env [inc/env.h](file:///c:/Users/rusla/OneDrive/%D0%A0%D0%B0%D0%B1%D0%BE%D1%87%D0%B8%D0%B9%20%D1%81%D1%82%D0%BE%D0%BB/OS-Course-2025/inc/env.h#L61-L85):
  - Для recv: bool env_ipc_recving уже есть; добавим uint64_t env_ipc_deadline_ms.
  - Для send: bool env_ipc_sending; envid_t env_ipc_send_target; uintptr_t env_ipc_send_srcva; size_t env_ipc_send_size; int env_ipc_send_perm; uint32_t env_ipc_send_value; uint64_t env_ipc_send_deadline_ms.
- Все поля модифицируются под lock_kernel() для атомарности в многопоточном окружении.

## Ядро: логика таймаута
- Добавим функцию ipc_timeout_tick(), которая:
  - Обходит envs, проверяет истекшие дедлайны для ожидающих recv и send.
  - Для recv-ожидания: если истек дедлайн и не пришло сообщение (env_ipc_from == 0), установить tf->reg_rax = -E_TIMEDOUT, env_ipc_recving = false, перевести ENV в ENV_RUNNABLE.
  - Для send-ожидания: если доступен получатель (env_ipc_recving == true) — вызывает существующий sys_ipc_try_send(); при успехе tf->reg_rax = 0 и пробуждает отправителя; при E_BAD_ENV пробуждает с ошибкой; если дедлайн истек и отправить не удалось — tf->reg_rax = -E_TIMEDOUT и пробуждает отправителя.
- Вставим вызов ipc_timeout_tick() в обработчике таймера [kern/trap.c](file:///c:/Users/rusla/OneDrive/%D0%A0%D0%B0%D0%B1%D0%BE%D1%87%D0%B8%D0%B9%20%D1%81%D1%82%D0%BE%D0%BB/OS-Course-2025/kern/trap.c#L322-L331) перед sched_yield(), чтобы гарантировать прогресс ожидающих.
- Источник времени: используем монотоник «сейчас» в миллисекундах как vsys_gettime()*1000 с шагом прерываний RTC/HPET. Гранулярность миллисекунд будет квантована частотой таймера (без busy waiting/цыкл опроса). При наличии HPET — шаг будет более тонким.

## Реализации системных вызовов
- sys_ipc_recv(dstva, maxsize, timeout_ms):
  - Валидация адреса и размера (как сейчас). Установка env_ipc_* полей, env_status = ENV_NOT_RUNNABLE, env_ipc_recving = true, вычисление env_ipc_deadline_ms = now_ms + timeout_ms. Возврат 0; фактический возврат произойдет после пробуждения.
- sys_ipc_send(envid, value, srcva, size, perm, timeout_ms):
  - Быстрая попытка через sys_ipc_try_send(); при -E_IPC_NOT_RECV — подготовить ожидание: записать поля отправки в Env, env_status = ENV_NOT_RUNNABLE, env_ipc_sending = true, env_ipc_send_deadline_ms = now_ms + timeout_ms.
  - При недоступности получателя (E_BAD_ENV) — немедленный возврат ошибки.

## Обработка прерываний и недоступности получателя
- Таймерные прерывания регулярно вызывают ipc_timeout_tick(), обеспечивая пробуждение по истечению таймаута и по готовности получателя.
- Если получатель уничтожен или недоступен — возвращаем -E_BAD_ENV отправителю (в tick-пути или сразу в sys_ipc_send).

## Пользовательская библиотека
- Обновим обертки:
  - sys_ipc_recv(void *dstva, size_t size, uint32_t timeout_ms).
  - sys_ipc_send(envid_t envid, uint64_t value, void *srcva, size_t size, int perm, uint32_t timeout_ms).
  - Сохранение ASAN-поведения: unpoison dstva на успех recv согласно thisenv->env_ipc_maxsz.
- Добавим удобные функции:
  - ipc_recv_timeout(..., timeout_ms) и ipc_send_timeout(..., timeout_ms) в [lib/ipc.c](file:///c:/Users/rusla/OneDrive/%D0%A0%D0%B0%D0%B1%D0%BE%D1%87%D0%B8%D0%B9%20%D1%81%D1%82%D0%BE%D0%BB/OS-Course-2025/lib/ipc.c#L1-L68), использующие блокирующие syscalls без busy waiting.
  - Оставим текущую ipc_send как совместимую (busy wait) для старых задач, но тесты будут использовать timeout-версии.

## Тесты
- user/tests_ipc_timeout_recv.c: проверка успешного приема, истечения таймаута при отсутствии отправителя, корректной ошибки.
- user/tests_ipc_timeout_send.c: проверка отправки с таймаутом, ситуации «получатель недоступен», успешной отправки когда получатель появляется позже.
- user/tests_ipc_multithread.c: несколько отправителей к одному получателю, проверка пробуждения и таймаутов без гонок.
- Все тесты используют spawn()/exofork и синхронизацию через новые API; проверяют коды ошибок (-E_TIMEDOUT, -E_BAD_ENV) и отсутствие утечек.

## Требования качества
- Без busy waiting: ожидание только через блокировку ENV_NOT_RUNNABLE и пробуждение из прерываний.
- Атомарность: все критические секции (чтение/запись Env/очереди) под lock_kernel().
- Обработка ошибок: полные проверки параметров, корректные коды возврата, согласованность со стилем проекта.
- ASAN/UBSAN: сохраняем существующие user_mem_assert/map_region проверки; не используем небезопасные указатели; следим за корректным poisoning/unpoisoning.
- Комментарии в сложных местах ядра (указание причин обновления Trapframe и порядка пробуждения).

## Примечания по точности таймаута
- Миллисекундный таймаут квантуется частотой таймерных прерываний (RTC/HPET). В конфигурации с HPET гранулярность выше, с RTC — ~500ms. При необходимости последующим патчем можно повысить частоту таймера.

Готов применить изменения и добавить тесты после подтверждения.