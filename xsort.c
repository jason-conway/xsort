/**
 * @file xsort.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Context-supported sorting for `frappe`
 * @version 23.03
 * @date 2023-03-08
 *
 * @copyright Copyright (c) 2023-2025 Jason Conway. All rights reserved.
 *
 */

#include "xsort.h"


#pragma region Allocator

static void *xmalloc(size_t size)
{
    void *ptr = malloc(size);
    if (unlikely(!ptr)) {
        abort();
    }
    return ptr;
}

static void *xcalloc(size_t count, size_t size)
{
    void *ptr = calloc(count, size);
    if (unlikely(!ptr)) {
        abort();
    }
    return ptr;
}

static void *xrealloc(void *ptr, size_t size)
{
    void *ptr = realloc(ptr, size);
    if (unlikely(!ptr)) {
        abort();
    }
    return ptr;
}

static void xfree(void *ptr)
{
    free(ptr);
}

#pragma endregion


typedef struct segment_t {
    size_t lh;
    size_t q1;
    size_t q2;
    size_t rh;
    size_t q3;
    size_t q4;
} segment_t;

static inline void rotate(uint64_t *restrict array, uint64_t *restrict swap, size_t left, size_t right)
{
    memcpy(&swap[0], &array[0], left * sizeof(uint64_t));
    memmove(&array[0], &array[left], right * sizeof(uint64_t));
    memcpy(&array[right], &swap[0], left * sizeof(uint64_t));
}

static inline void partition_array(size_t elements, segment_t *seg)
{
    uint64_t left = elements / 2;
    uint64_t right = elements - left;
    *seg = (segment_t) {
        .lh = left,
        .q1 = left / 2,
        .q2 = left - (left / 2),
        .rh = right,
        .q3 = right / 2,
        .q4 = right - (right / 2)
    };
}

// Conditionally swap `&seg[0]` and `&seg[1]` without branching
#define xchg_ctx(seg, cmp, arg) \
    ({ \
        __unused uint8_t _res = cmp(&seg[0], &seg[1], arg) > 0; \
        uint64_t _swap = seg[!_res]; \
        seg[0] = seg[_res]; \
        seg[1] = _swap; \
        _res; \
    })

static void oddeven_sort_ctx(uint64_t *restrict seg, size_t elements, cmp_ctx_fn_t cmp, void *arg)
{
    uint64_t *pair;
    switch (elements) {
        default: {
            // Alternate between even and odd iterations
            bool z = true;
            uint8_t itr = 1;
            // Current element being compared, starting from the third last element
            uint64_t *elem = &seg[elements - 3]; // 4, 5, 6, 7 => 1, 2, 3, 4,
            do {
                // Set pair to point to the adjacent element based on the current iteration (even or odd)
                pair = &elem[(z = !z)];
                do {
                    // Compare and swap adjacent elements if necessary, update itr accordingly
                    itr |= xchg_ctx(pair, cmp, arg);
                    // Move to the previous pair of elements
                    pair = &pair[-2];
                } while (pair >= seg);
            } while (itr-- && --elements);
            return;
        }
        case 3:
            // Compare and swap the first two elements if necessary
            pair = &seg[0];
            xchg_ctx(pair, cmp, arg);
            // Move to the next pair of elements and compare and swap if necessary
            pair = &pair[1];
            if (!xchg_ctx(pair, cmp, arg)) {
                return;
            }
            // fallthrough
        case 2:
            pair = &seg[0];
            xchg_ctx(pair, cmp, arg);
            // fallthrough
        case 1:
        case 0:
            return;
    }
}

// Merge the two subarrays within `src` into `dst`.
// The left subarray spans `&src[0]` to `src[left - 1]`, while the
// right subarray spans `&src[left]` to `&src[left + right - 1]`.
static void parity_merge_ctx(uint64_t *restrict src, uint64_t *restrict dst, size_t left, size_t right, cmp_ctx_fn_t cmp, void *arg)
{
    size_t i_ptd = 0;
    size_t i_tpd = left + right - 1;

    size_t i_ptl = 0;
    size_t i_ptr = left;

    if (left < right) {
        dst[i_ptd++] = cmp(&src[i_ptl], &src[i_ptr], arg) <= 0 ? src[i_ptl++] : src[i_ptr++];
    }

    size_t i_tpl = left - 1;
    size_t i_tpr = left + right - 1;

    while (--left) {
        dst[i_ptd++] = cmp(&src[i_ptl], &src[i_ptr], arg) <= 0 ? src[i_ptl++] : src[i_ptr++];
        dst[i_tpd--] = cmp(&src[i_tpl], &src[i_tpr], arg) > 0 ? src[i_tpl--] : src[i_tpr--];
    }

    dst[i_tpd] = cmp(&src[i_tpl], &src[i_tpr], arg) > 0 ? src[i_tpl] : src[i_tpr];
    dst[i_ptd] = cmp(&src[i_ptl], &src[i_ptr], arg) <= 0 ? src[i_ptl] : src[i_ptr];
}

void xsort(void *ptr, size_t elements, cmp_ctx_fn_t cmp, void *arg)
{
    typedef struct stack_frame_t {
        uint64_t *data;
        size_t elements;
        segment_t segment;
        void *ret_addr;
    } stack_frame_t;

    uint64_t *data = ptr;

    uint64_t *swap = xmalloc(elements * sizeof(uint64_t));

    ptrdiff_t stack_depth = 0;
    ptrdiff_t frame_capacity = 128;

    stack_frame_t *stk = xmalloc(frame_capacity * sizeof(stack_frame_t));

    stk[stack_depth] = (stack_frame_t) {
        .data = data,
        .elements = elements,
        .segment = { 0 },
        .ret_addr = &&ret_addr_0,
    };

    while (stack_depth >= 0) {
        if (unlikely(stack_depth + 2 >= frame_capacity)) {
            frame_capacity *= 2;
            stk = xrealloc(stk, frame_capacity * sizeof(stack_frame_t));
            fprintf(stderr, "[warning] resizing %s stack for %" PRIiPTR " frames\n", __FUNCTION__, frame_capacity);
        }

        stack_frame_t stk_top = stk[stack_depth--];
        data = stk_top.data;
        elements = stk_top.elements;
        segment_t segment = stk_top.segment;
        goto *stk_top.ret_addr;

ret_addr_0:
        if (elements <= 7) {
            oddeven_sort_ctx(data, elements, cmp, arg);
            continue;
        }

        partition_array(elements, &segment);

        stk[++stack_depth] = (stack_frame_t) {
            .data = &data[0],
            .elements = elements,
            .segment = segment,
            .ret_addr = &&ret_addr_1
        };
        stk[++stack_depth] = (stack_frame_t) {
            .data = &data[0],
            .elements = segment.q1,
            .segment = segment,
            .ret_addr = &&ret_addr_0
        };
        continue;
ret_addr_1:
        stk[++stack_depth] = (stack_frame_t) {
            .data = &data[0],
            .elements = elements,
            .segment = segment,
            .ret_addr = &&ret_addr_2
        };
        stk[++stack_depth] = (stack_frame_t) {
            .data = &data[segment.q1],
            .elements = segment.q2,
            .segment = segment,
            .ret_addr = &&ret_addr_0
        };
        continue;
ret_addr_2:
        stk[++stack_depth] = (stack_frame_t) {
            .data = &data[0],
            .elements = elements,
            .segment = segment,
            .ret_addr = &&ret_addr_3
        };
        stk[++stack_depth] = (stack_frame_t) {
            .data = &data[segment.lh],
            .elements = segment.q3,
            .segment = segment,
            .ret_addr = &&ret_addr_0
        };
        continue;
ret_addr_3:
        stk[++stack_depth] = (stack_frame_t) {
            .data = &data[0],
            .elements = elements,
            .segment = segment,
            .ret_addr = &&ret_addr_4
        };
        stk[++stack_depth] = (stack_frame_t) {
            .data = &data[segment.lh + segment.q3],
            .elements = segment.q4,
            .segment = segment,
            .ret_addr = &&ret_addr_0
        };
        continue;
ret_addr_4:
        if (cmp(&data[segment.q1 - 1], &data[segment.q1], arg) <= 0) {
            if (cmp(&data[segment.lh - 1], &data[segment.lh], arg) <= 0) {
                if (cmp(&data[segment.lh + segment.q3 - 1], &data[segment.lh + segment.q3], arg) <= 0) {
                    continue;
                }
            }
        }

        if (cmp(&data[0], &data[segment.lh - 1], arg) > 0) {
            if (cmp(&data[segment.q1], &data[segment.lh + segment.q3 - 1], arg) > 0) {
                if (cmp(&data[segment.lh], &data[elements - 1], arg) > 0) {
                    rotate(&data[0], &swap[0], segment.q1, segment.q2 + segment.rh);
                    rotate(&data[0], &swap[0], segment.q2, segment.rh);
                    rotate(&data[0], &swap[0], segment.q3, segment.q4);
                    continue;
                }
            }
        }

        parity_merge_ctx(&data[0], &swap[0], segment.q1, segment.q2, cmp, arg);
        parity_merge_ctx(&data[segment.lh], &swap[segment.lh], segment.q3, segment.q4, cmp, arg);
        parity_merge_ctx(&swap[0], &data[0], segment.lh, segment.rh, cmp, arg);
    }
    xfree(swap);
    xfree(stk);
}
