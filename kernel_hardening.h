#ifndef KERNEL_HARDENING_H
#define KERNEL_HARDENING_H

/*
 * Minimal hardening layer for the monolithic kernel.
 *
 * The project is a single-file freestanding kernel. Directly rewriting the whole
 * kernel.c in-place is brittle because the file is enormous and several sections
 * are already interdependent. This header adds safe helper primitives that can be
 * adopted incrementally by the existing code paths without changing the overall
 * architecture.
 */

#define KERNEL_SAFE_TRUE  1
#define KERNEL_SAFE_FALSE 0

static inline int kernel_u32_add_overflow(uint32_t a, uint32_t b, uint32_t *out) {
    uint64_t sum = (uint64_t)a + (uint64_t)b;
    if (out) *out = (uint32_t)sum;
    return sum > 0xFFFFFFFFu;
}

static inline int kernel_size_checked_copy(uint8_t *dst,
                                          uint32_t dst_cap,
                                          const uint8_t *src,
                                          uint32_t src_len) {
    uint32_t avail;
    if (!dst || !src) return KERNEL_SAFE_FALSE;
    if (dst_cap == 0u || src_len == 0u) return KERNEL_SAFE_FALSE;
    if (src_len > dst_cap) return KERNEL_SAFE_FALSE;
    avail = dst_cap;
    if (src_len > avail) return KERNEL_SAFE_FALSE;
    for (uint32_t i = 0; i < src_len; ++i) dst[i] = src[i];
    return KERNEL_SAFE_TRUE;
}

static inline int kernel_safe_strncpy(char *dst,
                                      uint32_t dst_cap,
                                      const char *src,
                                      uint32_t max_len) {
    uint32_t i;
    if (!dst || !src || dst_cap == 0u) return KERNEL_SAFE_FALSE;
    if (max_len >= dst_cap) max_len = dst_cap - 1u;
    for (i = 0; i < max_len; ++i) {
        dst[i] = src[i];
        if (src[i] == '\0') return KERNEL_SAFE_TRUE;
    }
    dst[max_len] = '\0';
    return KERNEL_SAFE_TRUE;
}

static inline int kernel_safe_lba_range(uint32_t lba,
                                       uint32_t count,
                                       uint32_t total_sectors,
                                       uint32_t min_lba,
                                       uint32_t max_lba) {
    uint32_t end;
    if (count == 0u) return KERNEL_SAFE_TRUE;
    if (lba < min_lba) return KERNEL_SAFE_FALSE;
    if (lba > max_lba) return KERNEL_SAFE_FALSE;
    if (count > (max_lba - lba + 1u)) return KERNEL_SAFE_FALSE;
    end = lba + count;
    if (end > total_sectors) return KERNEL_SAFE_FALSE;
    return KERNEL_SAFE_TRUE;
}

static inline int kernel_safe_file_name(const char *name,
                                       uint32_t name_cap,
                                       uint32_t max_name_len) {
    uint32_t i = 0;
    if (!name || name_cap == 0u) return KERNEL_SAFE_FALSE;
    while (i < name_cap && i < max_name_len && name[i] != '\0') {
        if (name[i] == '/' || name[i] == '\\') return KERNEL_SAFE_FALSE;
        ++i;
    }
    return KERNEL_SAFE_TRUE;
}

#endif /* KERNEL_HARDENING_H */
