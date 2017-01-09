#include <syscall.h>
#include <int.h>
#include <unistd.h>
#include <ps.h>
#include <ps0.h>
#include <config.h>
#include <timer.h>
#include <klib.h>
#include <rbtree.h>
#include <fcntl.h>
#include <errno.h>
#include <fs.h>
#include <select.h>

// FIXME
// no signal at all
// no except
int do_select(int nfds, fd_set *readfds, fd_set *writefds,
                   fd_set *exceptfds, const struct timespec *timeout,
                   void *sigmask)
{
    fd_set *reads, *writes, *excepts;
    int ret = EOK;
    int i = 0;
    unsigned time_begin = time_now(); // milliseconds
    unsigned time_span = 0;
    int infinit_wait = 0;
    unsigned wait_time = 0; // milliseconds
    reads = writes = excepts = NULL;

#define ALLOC_SRC(src, dst)\
    if (dst){                               \
        src = calloc(1, sizeof(fd_set));    \
        memcpy(src, dst, sizeof(fd_set));   \
        FD_ZERO(dst);                       \
    }

    ALLOC_SRC(reads, readfds);
    ALLOC_SRC(writes, writefds);
    ALLOC_SRC(excepts, exceptfds);

#undef ALLOC_SRC

    if (timeout){
        wait_time = timeout->tv_sec*1000 + timeout->tv_nsec/1000;
    }else{
        infinit_wait = 1;
    }

#define CHECK_FDS(src, dst, type)\
        for (i = 0; src && (i < nfds); i++){    \
            if (FD_ISSET(i, src)){              \
                if (fd_select(i, type) == 0){   \
                    FD_SET(i, dst);             \
                    has_set = 1;                \
                }                               \
            }                                   \
        }

    do{
        // check read fds
        int has_set = 0;
        CHECK_FDS(reads, readfds, FS_SELECT_READ);

        // check write fds
        CHECK_FDS(writes, writefds, FS_SELECT_WRITE);

        // check except fds
        CHECK_FDS(excepts, exceptfds, FS_SELECT_EXCEPT);

        if (has_set){
            ret = EOK;
            break;
        }

        if (infinit_wait){
            task_sched();
            continue;
        }
        
        time_span = time_now() - time_begin;
        if (time_span > wait_time){
            ret = -ETIMEDOUT;
            break;
        }

        task_sched();
    }while(1);

#undef CHECK_FDS

done:
    if (reads)
        free(reads);
    
    if (writes)
        free(writes);

    return ret;
}