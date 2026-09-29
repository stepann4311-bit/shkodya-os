# Shkodya OS Kernel Safety Audit

## Резюме
**Уровень критичности: ВЫСОКИЙ** — найдено 7 критических ошибок, которые могут привести к:
- Переполнению буфера и краже данных
- Повреждению памяти и сбоям системы
- Уязвимостям безопасности

---

## 🔴 КРИТИЧЕСКИЕ ОШИБКИ

### 1. **Переполнение буфера в `shk_copy()` — КРИТИЧНО**

**Файл:** `kernel.c:871–875`

```c
static void shk_copy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;  // ❌ НЕТ проверок границ буфера!
}
```

**Проблема:**
- Функция не проверяет, существуют ли буферы `dst` и `src`
- Нет проверки переполнения при копировании
- Вызывается по всему коду без валидации параметров

**Примеры опасного использования:**
- `shk_copy(shk_fs_root.magic, shk_fs_magic, 8)` — если `src` указывает за границы памяти, произойдёт чтение из невалидной области
- `shk_copy(out_names[count++], shk_fs_root.entries[i].filename, 16)` — возможно переполнение `out_names`

**Исправление:**
```c
static int shk_copy_safe(void *dst, const void *src, uint32_t n, uint32_t dst_cap) {
    if (!dst || !src || n == 0 || n > dst_cap) return SHK_EINVAL;
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return SHK_OK;
}
```

---

### 2. **Недостаточная валидация размеров в `fs_write_file()` — КРИТИЧНО**

**Файл:** `kernel.c:1000–1050` (строки не полны в выводе)

**Проблема:**
- Функция записи файла не проверяет `size` перед выделением LBA
- Возможна запись за пределы выделенного пространства

**Сценарий атаки:**
```c
fs_write_file("x.txt", large_buffer, 0xFFFFFFFF);  // size близ к uint32_t_MAX
// Allocation может переполниться: start_lba + count > total_sectors
```

**Исправление:**
```c
int fs_write_file(const char *name, const uint8_t *data, uint32_t size) {
    // ... existing validation ...
    
    // ADD THIS CHECK:
    uint32_t required_sectors = shk_fs_sectors(size);
    if (required_sectors == 0 && size > 0) return SHK_ERANGE;  // overflow
    if (required_sectors > (shk_fs_root.total_sectors - SHK_FS_DATA_LBA))
        return SHK_ENOSPC;
    
    // ... rest of function ...
}
```

---

### 3. **Целочисленное переполнение в `shk_fs_allocate()` — КРИТИЧНО**

**Файл:** `kernel.c:994–1010`

```c
static int shk_fs_allocate(uint32_t count, uint32_t *start) {
    uint32_t pos = SHK_FS_DATA_LBA, i;
    if (!count) { *start = 0; return SHK_OK; }
    for (;;) {
        if (pos >= shk_fs_root.total_sectors ||
            count > shk_fs_root.total_sectors - pos) return SHK_ENOSPC;
        // ❌ pos + count может переполниться, если pos близко к UINT32_MAX
```

**Проблема:**
- Проверка `pos >= ... || count > ...` работает, но результат `pos + count` не проверяется на переполнение
- Если `pos = 0xFFFFFFF0` и `count = 0x20`, то `pos + count = 0x10` (переполнение!)

**Исправление:**
```c
static int shk_fs_allocate(uint32_t count, uint32_t *start) {
    uint32_t pos = SHK_FS_DATA_LBA, i, end;
    if (!count) { *start = 0; return SHK_OK; }
    for (;;) {
        // Check overflow: pos + count > UINT32_MAX
        if (pos > 0xFFFFFFFFu - count) return SHK_ENOSPC;
        
        end = pos + count;  // Now safe
        if (end > shk_fs_root.total_sectors) return SHK_ENOSPC;
        
        // ... rest of allocation logic ...
    }
}
```

---

### 4. **Отсутствие проверки NULL в `fs_list()` — ВЫСОКИЙ**

**Файл:** `kernel.c:982–992`

```c
int fs_list(char out_names[][16]) {
    // ...
    if (!out_names) return SHK_EINVAL;  // ✓ Хорошо
    shk_zero(out_names, SHK_FS_MAX_FILES * SHK_FS_NAME_BYTES);
    // ❌ Но shk_copy НЕ проверяет NULL!
    shk_copy(out_names[count++], shk_fs_root.entries[i].filename, 16);
```

**Проблема:**
- `out_names[count++]` может быть невалидным указателем если `count` переполнится
- Нет границ проверки индекса

**Исправление:**
```c
int fs_list(char out_names[][16]) {
    uint32_t i;
    int count = 0;
    if (shk_fs_state < 0) return shk_fs_state;
    if (!out_names) return SHK_EINVAL;
    shk_zero(out_names, SHK_FS_MAX_FILES * SHK_FS_NAME_BYTES);
    for (i = 0; i < SHK_FS_MAX_FILES; ++i) {
        if (shk_fs_root.entries[i].is_used) {
            if (count >= SHK_FS_MAX_FILES) return SHK_EBUFFER;  // overflow check
            shk_copy(out_names[count++], shk_fs_root.entries[i].filename, 16);
        }
    }
    return count;
}
```

---

### 5. **Race Condition между проверкой и использованием в ATA операциях — ВЫСОКИЙ**

**Файл:** `kernel.c:790–844`

```c
static int shk_ata_begin(uint32_t lba, uint8_t command) {
    if (!ata_present) return SHK_ENODEV;  // ← Check (T1)
    // ... delay, I/O operations ...
    // ❌ ata_present может измениться в interrupt между T1 и сейчас
    outb(SHK_ATA_COMMAND, command);  // ← Use (T2)
}
```

**Проблема:**
- Между проверкой `ata_present` и реальным использованием диска диск может быть отключен
- Нет механизма блокировки доступа

**Исправление:**
```c
// Add simple spinlock:
static volatile int ata_lock = 0;

static int shk_ata_acquire(void) {
    uint32_t timeout = 1000000u;
    while (ata_lock && timeout--) ;
    if (timeout == 0) return SHK_ETIMEOUT;
    ata_lock = 1;
    return SHK_OK;
}

static void shk_ata_release(void) {
    ata_lock = 0;
}

// Use in shk_ata_begin:
static int shk_ata_begin(uint32_t lba, uint8_t command) {
    int r = shk_ata_acquire();
    if (r < 0) return r;
    if (!ata_present) { shk_ata_release(); return SHK_ENODEV; }
    // ... rest ...
    shk_ata_release();
    return SHK_OK;
}
```

---

### 6. **Отсутствие проверки валидности имени файла — СРЕДНИЙ/ВЫСОКИЙ**

**Файл:** `kernel.c:890–898`

```c
static uint32_t shk_fs_name_length(const char *name) {
    uint32_t i;
    if (!name) return 0;
    for (i = 0; i < SHK_FS_NAME_BYTES; ++i) {
        if (name[i] == '\0') return i;
        if (name[i] == '/' || name[i] == '\\') return 0;  // ✓ Хорошо
    }
    return 0;  // ❌ Имя без нулевого терминатора проходит проверку!
}
```

**Проблема:**
- Если имя файла не содержит `\0` и не содержит `/` или `\`, функция вернёт 0
- Это может быть интерпретировано как пустое имя, но копирование всё равно произойдёт

**Исправление:**
```c
static uint32_t shk_fs_name_length(const char *name) {
    uint32_t i;
    if (!name) return 0;
    for (i = 0; i < SHK_FS_NAME_BYTES; ++i) {
        if (name[i] == '\0') return i;  // Valid: found terminator
        if (name[i] == '/' || name[i] == '\\') return 0;  // Invalid chars
    }
    return 0;  // No terminator found in SHK_FS_NAME_BYTES bytes -> invalid
}

// Add this check before using names:
static int shk_fs_name_valid(const char *name) {
    if (!name) return 0;
    if (shk_fs_name_length(name) == 0) return 0;  // Empty or invalid
    return 1;
}
```

---

### 7. **Недостаточная валидация размера буфера при чтении в `vfs_read_text()` — ВЫСОКИЙ**

**Файл:** `kernel.c` (в строках для чтения файлов UI)

**Проблема:**
- При чтении файла в UI буфер `vfs_text_buffer[VFS_FILE_SIZE]` имеет размер 8192 байта
- Но нет проверки, что файл не превышает этот размер перед копированием
- `VFS_FILE_SIZE` — это "текстовый буфер для staging", а не лимит дискового файла

**Сценарий:**
```c
// Если на диске файл 100 KB, а буфер 8 KB:
fs_read_file("bigfile.bin", vfs_text_buffer, VFS_FILE_SIZE);
// Будет прочитано только 8 KB, но система может ожидать всё
```

**Исправление:**
```c
// Пересчитать VFS_FILE_SIZE или добавить проверку в fs_read_file:
int fs_read_file(const char *name, uint8_t *buffer, uint32_t max_size) {
    // ... existing code ...
    
    if (buffer == NULL) return SHK_EINVAL;
    if (max_size == 0) return SHK_EINVAL;
    
    // Check that entry size doesn't exceed buffer
    if (e->size_bytes > max_size) {
        return SHK_EBUFFER;  // Buffer too small
    }
    
    // ... continue reading ...
}
```

---

## 📊 Таблица критичности

| # | Ошибка | Уровень | Тип | Решение |
|---|--------|---------|-----|---------|
| 1 | `shk_copy()` без проверок | 🔴 КРИТИЧ | Переполнение | Добавить проверки границ |
| 2 | `fs_write_file()` без валидации size | 🔴 КРИТИЧ | Переполнение LBA | Проверить SHK_ERANGE |
| 3 | Целочисленное переполнение в `shk_fs_allocate()` | 🔴 КРИТИЧ | Переполнение | Explicit overflow check |
| 4 | NULL-проверка в `fs_list()` | 🟠 ВЫСОКИЙ | Доступ за границы | Boundary check на count |
| 5 | Race condition в ATA | 🟠 ВЫСОКИЙ | TOCTOU | Добавить spinlock |
| 6 | Имя файла без null-terminator | 🟠 ВЫСОКИЙ | Buffer overflow | Valida name before use |
| 7 | Размер буфера VFS недостаточен | 🟠 ВЫСОКИЙ | Truncation | Проверить max_size |

---

## ✅ Рекомендации по приоритизации исправлений

### Приоритет 1 (Сделать ЧЕМ МОЖНО СКОРЕЕ):
1. Переписать `shk_copy()` с проверками (проще всего, максимальный эффект)
2. Добавить overflow check в `shk_fs_allocate()`
3. Валидировать имена файлов перед использованием

### Приоритет 2 (В следующем обновлении):
4. Добавить spinlock для ATA операций
5. Увеличить `VFS_FILE_SIZE` или добавить динамическое выделение памяти
6. Добавить проверку границ в `fs_list()`

### Приоритет 3 (Рефакторинг):
7. Рассмотреть разделение `kernel.c` на модули для лучшей тестируемости
8. Добавить unit-тесты для критических функций FS

---

## 🛡️ Примеры правильного кода

Файл `kernel_hardening.h` содержит вспомогательные функции:

```c
#include "kernel_hardening.h"

// Безопасное копирование:
if (kernel_size_checked_copy(dst, sizeof(dst), src, len) == KERNEL_SAFE_FALSE) {
    return SHK_EBUFFER;
}

// Безопасная проверка диапазона LBA:
if (kernel_safe_lba_range(lba, count, total, 0, max_lba) == KERNEL_SAFE_FALSE) {
    return SHK_ERANGE;
}

// Безопасная проверка имени:
if (kernel_safe_file_name(name, sizeof(name), 16) == KERNEL_SAFE_FALSE) {
    return SHK_EINVAL;
}
```

---

## Заключение

Shkodya OS — это амбициозный проект с красивым UI и интересной архитектурой, **но код не рассчитан на враждебный ввод**. Для боевого использования необходимо:

1. ✅ **Немедленно**: Добавить bounds checking в критические функции
2. ✅ **До release**: Провести security audit всех I/O операций
3. ✅ **Постоянно**: Использовать sanitizers и fuzzing для нового кода

Спецификация проекта («No deletion, no power-failure recovery») — честная, но нужна **честная же обработка ошибок**, а не молчаливое повреждение памяти.
