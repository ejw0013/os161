#ifndef _FILE_SYSCALLS_H_
#define _FILE_SYSCALLS_H_

#include <kern/limits.h>
#include <types.h>
#include <lib.h>
#include <synch.h>

#define MAX_FILES 256
#define FREE 0
#define TAKEN 1
/**
 * node_t is a node_t.
 */
typedef enum {
	READ_ONLY,
	WRITE_ONLY,
	READ_WRITE
} mode_t; 

struct file_info {
	char file_name[NAME_MAX];
	struct vnode *vn;
	unsigned int offset;
	struct lock* mutex;
	int flags;
	int refcount;
};

void changeppid(pid_t change, pid_t ppid);

/**
 * initialize STDIN, OUT, ERR
 */
int filetable_init();
/**
 * open the file.
 */
int sys_open(const char* path, int oflag, mode_t mode, int* fd);

/**
 * read the file.
 */
int sys_read(int fd, void* buf, size_t nbytes);

/**
 * write the file.
 */
int sys_write(int fd, const void* buf, size_t nbytes);

/**
 * lseek the file.
 */
int sys_lseek(int fd, off_t offset, int whence, int* ret);

/**
 * close the file.
 */
int sys_close(int fd, int* ret);

/**
 * dup2 the file.
 *
 * lmow.
 */
int sys_dup2(int oldfd, int newfd, int* ret);

#endif
