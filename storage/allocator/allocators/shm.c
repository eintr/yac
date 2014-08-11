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

#ifdef USE_SHM

#if defined(__FreeBSD__)
# include <machine/param.h>
#endif
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/stat.h>
#include <fcntl.h>

static yac_shared_memory_t attach(unsigned long size) /* {{{ */ {
	yac_shared_memory_t ret;
	unsigned int pagesize, real_size;

	pagesize = sysconf(_SC_PAGE_SIZE);

	if (size%pagesize!=0) {
		size = (size/pagesize+1)*pagesize;
	}

	ret.allocator_info.i = shmget(IPC_PRIVATE, size, 0600);
	if (ret.allocator_info.i<0) {
		ret.ptr = NULL;
		ret.size = 0;
		ret.allocator_info.i = -1;
	} else {
		ret.ptr = shmat(ret.allocator_info.i, NULL, 0);
		if (ret.ptr == (void*)-1) {
			ret.ptr = NULL;
			ret.size = 0;
			shmctl(ret.allocator_info.i, IPC_RMID, NULL);
			ret.allocator_info.i = -1;
		} else {
			ret.size = size;
		}
	}

	return ret;
}

static int detach(yac_shared_memory_t *shm) /* {{{ */ {
	shmdt(shm->ptr);
	return shmctl(shm->allocator_info.i, IPC_RMID, NULL);
}
/* }}} */

yac_shared_memory_handlers yac_alloc_shm_handlers = /* {{{ */ {
	attach,
	detach,
};
/* }}} */

#endif /* USE_SHM */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
