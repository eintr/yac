/*
   +----------------------------------------------------------------------+
   | Yet Another Cache                                                    |
   +----------------------------------------------------------------------+
   | Copyright (c) 2013-2013 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Xinchen Hui <laruence@php.net>                              |
   +----------------------------------------------------------------------+
*/

#include <errno.h>
#include <time.h>
#include <sys/types.h>

#include "php.h"
#include "storage/yac_storage.h"
#include "yac_allocator.h"

static const yac_shared_memory_handlers *shared_memory_handler = NULL;
static const char *shared_model;

static inline unsigned int yac_storage_align_size(unsigned int size) /* {{{ */ {
    int bits = 0;
    while ((size = size >> 1)) {
        ++bits;
    }
    return (1 << bits);
}
/* }}} */

int yac_allocator_startup(unsigned long ksize, unsigned long vsize, unsigned long flags, char **msg) /* {{{ */ {
	unsigned int i;
	const yac_shared_memory_handlers *h;
	yac_shared_segment *segments;

    if (!(h = &yac_shared_memory_handler)) {
        return ALLOC_FAILURE;
    }

	yac_storage = h->attach(sizeof(yac_storage));
	if (yac_storage.ptr==NULL) {
		return ALLOC_FAILURE;
	}
	YAC_SG(segments_num)= 1024;

	YAC_SG(value_segment) = h->attach(vsize);
	if (YAC_SG(value_segment).ptr == NULL) {
		// TODO: detach
		return ALLOC_FAILURE;
	}
    while ((YAC_SG(value_segment).size / YAC_SG(segments_num)) < YAC_SMM_SEGMENT_MIN_SIZE) {
        YAC_SG(segments_num) >>= 1;
		if (YAC_SG(segments_num)==0) {
			YAC_SG(segments_num) = 1;
			break;
		}
    }
    YAC_SG(segments_size) = YAC_SG(value_segment).size / YAC_SG(segments_num);
	YAC_SG(segments) = calloc(YAC_SG(segments_num), sizeof(yac_shared_segment*));
	if (YAC_SG(segments)==NULL) {
		// TODO: detach
		return ALLOC_FAILURE;
	}
	segments = calloc(YAC_SG(segments_num), sizeof(yac_shared_segment));
	for (i=0; i<YAC_SG(segments_num); ++i) {
		YAC_SG(segments)[i] = segments+i;
		YAC_SG(segments)[i]->pos = 0;
		YAC_SG(segments)[i]->p = YAC_SG(value_segment).ptr + i*YAC_SMM_ALIGNED_SIZE(YAC_SG(segments_size));
		if (i<YAC_SG(segments_num)-1) {
			YAC_SG(segments)[i]->size = YAC_SMM_ALIGNED_SIZE(YAC_SG(segments_size));
		} else {
			YAC_SG(segments)[i]->size = YAC_SG(value_segment).size-i*YAC_SMM_ALIGNED_SIZE(YAC_SG(segments_size));
		}
	}

	YAC_SG(key_segment) = h->attach(ksize);
	if (YAC_SG(key_segment).ptr == NULL) {
		// TODO: detach
		return ALLOC_FAILURE;
	}
	YAC_SG(slots) = YAC_SG(key_segment).ptr;
	YAC_SG(slots_size) = yac_storage_align_size(YAC_SG(key_segment).size / sizeof(yac_kv_key));
	if (!((YAC_SG(key_segment).size / sizeof(yac_kv_key)) & ~(YAC_SG(slots_size) << 1))) {
		YAC_SG(slots_size) <<= 1;
	}
	YAC_SG(slots_mask) 	= YAC_SG(slots_size) - 1;
    YAC_SG(slots_num)  	= 0;

	if (flags&YAC_FLAGS_USE_LOCK) {
		YAC_SG(slots_mutex_segment) = h->attach(sizeof(yac_mutexarray_t)+YAC_SG(slots_size)*sizeof(int));
		YAC_SG(slots_mutex) = YAC_SG(slots_mutex_segment).ptr;
		if (YAC_SG(slots_mutex) == NULL) {
			// TODO: detach
			return ALLOC_FAILURE;
		}
		YAC_SG(slots_mutex)->nelms = YAC_SG(slots_size);
		yac_mutexarray_init(YAC_SG(slots_mutex));
	} else {
		YAC_SG(slots_mutex) = NULL;
	}

	return ALLOC_SUCCESS;
}
/* }}} */

void yac_allocator_shutdown(void) /* {{{ */ {
	const yac_shared_memory_handlers *h;

	yac_mutexarray_destroy(YAC_SG(slots_mutex));

    if (!(h = &yac_shared_memory_handler)) {
        return;
    }
	h->detach(&YAC_SG(key_segment));
	h->detach(&YAC_SG(value_segment));
	if (YAC_SG(slots_mutex)) {
		h->detach(&YAC_SG(slots_mutex_segment));
		YAC_SG(slots_mutex)=NULL;
	}
}
/* }}} */

static inline void *yac_allocator_alloc_algo2(unsigned long size, int hash) /* {{{ */ {
    yac_shared_segment *segment;
	unsigned int seg_size, retry, pos, current;

	current = hash & YAC_SG(segments_num_mask);
	/* do we really need lock here? it depends the real life exam */
	retry = 3;
do_retry:
    segment = YAC_SG(segments)[current];
	seg_size = segment->size;
	pos = segment->pos;
	if ((seg_size - pos) >= size) {
do_alloc:
		pos += size;
		segment->pos = pos;
		if (segment->pos == pos) {
			return (void *)((char *)segment->p + (pos - size));
		} else if (retry--) {
			goto do_retry;
		}
		return NULL;
    } else { 
		int i, max;
		max = (YAC_SG(segments_num) > 4)? 4 : YAC_SG(segments_num);
		for (i = 1; i < max; i++) {
			segment = YAC_SG(segments)[(current + i) & YAC_SG(segments_num_mask)];
			seg_size = segment->size;
			pos = segment->pos;
			if ((seg_size - pos) >= size) {
				current = (current + i) & YAC_SG(segments_num_mask);
				goto do_alloc;
			}
		}
//		segment->pos = 0;
		pos = 0;
		++YAC_SG(recycles);
		goto do_alloc;
	}
}
/* }}} */

#if 0
static inline void *yac_allocator_alloc_algo1(unsigned long size) /* {{{ */ {
    int i, j, picked_seg, atime;
    picked_seg = (YAC_SG(current_seg) + 1) & YAC_SG(segments_num_mask);

    atime = YAC_SG(segments)[picked_seg]->atime;
    for (i = 0; i < 10; i++) {
        j = (picked_seg + 1) & YAC_SG(segments_num_mask);
        if (YAC_SG(segments)[j]->atime < atime) {
            picked_seg = j;
            atime = YAC_SG(segments)[j]->atime;
        }
    }

    YAC_SG(current_seg) = picked_seg;
    YAC_SG(segments)[picked_seg]->pos = 0;
    return yac_allocator_alloc_algo2(size);
}
/* }}} */
#endif

unsigned long yac_allocator_real_size(unsigned long size) /* {{{ */ {
	unsigned long real_size = YAC_SMM_TRUE_SIZE(size);

    if (real_size > YAC_SG(segments_size)) {
        return 0;
    }

	return real_size;
}
/* }}} */

void * yac_allocator_raw_alloc(unsigned long real_size, int hash) /* {{{ */ {

	return yac_allocator_alloc_algo2(real_size, hash);
	/*
    if (YAC_SG(exhausted)) {
        return yac_allocator_alloc_algo1(real_size);
    } else {
        void *p;
        if ((p = yac_allocator_alloc_algo2(real_size))) {
            return p;
        }
        return yac_allocator_alloc_algo1(real_size);
    }
	*/
}
/* }}} */

#if 0
void yac_allocator_touch(void *p, unsigned long atime) /* {{{ */ {
	yac_shared_block_header h = *(yac_shared_block_header *)(p - sizeof(yac_shared_block_header));

	if (h.seg >= YAC_SG(segments_num)) {
		return;
	}
	
	YAC_SG(segments)[h.seg]->atime = atime;
}
/* }}} */
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
