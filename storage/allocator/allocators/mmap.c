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

#include "storage/yac_storage.h"
#include "storage/allocator/yac_allocator.h"

#ifdef USE_MMAP

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#if defined(MAP_ANON) && !defined(MAP_ANONYMOUS)
# define MAP_ANONYMOUS MAP_ANON
#endif

#ifndef MAP_FAILED
#define MAP_FAILED (void *)-1
#endif

static yac_shared_memory_t attach(unsigned long size) /* {{{ */ {
	yac_shared_memory_t ret;
	unsigned int pagesize, real_size;

	pagesize = sysconf(_SC_PAGE_SIZE);

	if (size%pagesize!=0) {
		size = (size/pagesize+1)*pagesize;
	}

	ret.ptr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	if (ret.ptr==MAP_FAILED) {
		ret.ptr = NULL;
	}
	ret.size = size;
	ret.allocator_info.p = NULL;
	return ret;
}
/* }}} */

static int detach(yac_shared_memory_t *shm) /* {{{ */ {
	return !munmap(shm->ptr, shm->size);
}

yac_shared_memory_handlers yac_alloc_mmap_handlers = /* {{{ */ {
	attach,
	detach,
};
/* }}} */

#endif /* USE_MMAP */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
