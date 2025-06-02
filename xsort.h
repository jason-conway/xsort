/**
 * @file xsort.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Context-supported sorting for `frappe`
 * @version 23.03
 * @date 2023-03-08
 *
 * @copyright Copyright (c) 2023-2025 Jason Conway. All rights reserved.
 *
 */

#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#ifndef __has_builtin
    #define __has_builtin(x)
#endif

#ifndef __has_attribute
    #define __has_attribute(x)
#endif

#ifndef unlikely
    #if __has_builtin(__builtin_expect)
        #define unlikely(x) __builtin_expect(!!(x), 0)
    #else
        #define unlikely(x)
    #endif
#endif

#ifndef likely
    #if __has_builtin(__builtin_expect)
        #define likely(x) __builtin_expect(!!(x), 1)
    #else
        #define likely(x)
    #endif
#endif

#ifndef __unused
    #if __has_attribute(unused)
        #define __unused __attribute__((unused))
    #else
        #define __unused
    #endif
#endif

typedef ptrdiff_t (*cmp_ctx_fn_t)(const void *, const void *, void *);

// Sort array of 64-bit elements.
// Supports context-aware comparison via a user-provided
// comparison function `cmp` and an auxiliary argument `arg`
void xsort(void *ptr, size_t elements, cmp_ctx_fn_t cmp, void *arg);
