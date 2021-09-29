/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2013 Regents of the University of California
 */

#ifndef _ASM_RISCV_STRING_H
#define _ASM_RISCV_STRING_H

#ifdef CONFIG_CC_OPTIMIZE_FOR_PERFORMANCE
#define __HAVE_ARCH_MEMSET
extern void *memset(void *s, int c, size_t count);
extern void *__memset(void *s, int c, size_t count);
#define __HAVE_ARCH_MEMCPY
extern void *memcpy(void *dest, const void *src, size_t count);
extern void *__memcpy(void *dest, const void *src, size_t count);
#define __HAVE_ARCH_MEMMOVE
extern void *memmove(void *dest, const void *src, size_t count);
extern void *__memmove(void *dest, const void *src, size_t count);
#endif

/* For those files which don't want to check by kasan. */
#if defined(CONFIG_KASAN) && !defined(__SANITIZE_ADDRESS__)
#define memcpy(dst, src, len) __memcpy(dst, src, len)
#define memset(s, c, n) __memset(s, c, n)
#define memmove(dst, src, len) __memmove(dst, src, len)

#ifndef __NO_FORTIFY
#define __NO_FORTIFY /* FORTIFY_SOURCE uses __builtin_memcpy, etc. */
#endif

#endif
#endif /* _ASM_RISCV_STRING_H */
