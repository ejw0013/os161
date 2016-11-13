#include <types.h>
#include <kern/file_syscalls.h>
#include <lib.h>
#include <curthread.h>
#include <kern/limits.h>
#include <uio.h>
#include <kern/errno.h>
#include <vfs.h>
#include <synch.h>
#include <vnode.h>
#include <thread.h>
#include <kern/unistd.h>
#include <kern/stat.h>

int filetable_init(void) {
	struct vnode* vnin;
	struct vnode* vnout;
	struct vnode* vnerr;
	
	char* in = NULL;
	char* out = NULL;
	char* er = NULL;
	
	in = kstrdup("con:");
	out = kstrdup("con:");
	er = kstrdup("con:");

	if (vfs_open(in, O_RDONLY, &vnin)) {
		kfree(in);
		kfree(out);
		kfree(er);
		vfs_close(vnin);
		return EINVAL;
	}

	curthread->fdtable[0] = (struct file_info*) kmalloc(sizeof(struct file_info));
	curthread->fdtable[0]->vn = vnin;
	strcpy(curthread->fdtable[0]->file_name, "con:");
	curthread->fdtable[0]->flags = O_RDONLY;
	curthread->fdtable[0]->offset = 0;
	curthread->fdtable[0]->refcount = 1;
	curthread->fdtable[0]->mutex = lock_create(in);

	if (vfs_open(out, O_WRONLY, &vnout)) {
		kfree(in);
		kfree(out);
		kfree(er);
		lock_destroy(curthread->fdtable[0]->mutex);
		kfree(curthread->fdtable[0]);
		vfs_close(vnin);
		vfs_close(vnout);
		return EINVAL;
	}


	curthread->fdtable[1] = (struct file_info*) kmalloc(sizeof(struct file_info));
	curthread->fdtable[1]->vn = vnout;
	strcpy(curthread->fdtable[0]->file_name, "con:");
	curthread->fdtable[1]->flags = O_WRONLY;
	curthread->fdtable[1]->offset = 0;
	curthread->fdtable[1]->refcount = 1;
	curthread->fdtable[1]->mutex = lock_create(out);


	if (vfs_open(er, O_WRONLY, &vnerr)) {
		kfree(in);
		kfree(out);
		kfree(er);
		lock_destroy(curthread->fdtable[0]->mutex);
		kfree(curthread->fdtable[0]);
		lock_destroy(curthread->fdtable[1]->mutex);
		kfree(curthread->fdtable[1]);
		vfs_close(vnin);
		vfs_close(vnout);
		vfs_close(vnerr);
		return EINVAL;
	}

	curthread->fdtable[2] = (struct file_info*) kmalloc(sizeof(struct file_info));
	curthread->fdtable[2]->vn = vnerr;
	strcpy(curthread->fdtable[2]->file_name, "con:");
	curthread->fdtable[2]->flags = O_WRONLY;
	curthread->fdtable[2]->offset = 0;
	curthread->fdtable[2]->refcount = 1;
	curthread->fdtable[2]->mutex = lock_create(er);

	return 0;
}

int sys_open(const char* path, int oflag, mode_t mode, int* fd) {
	(void) mode;
	int result = 0, index = 3;
	struct vnode *vn;
	char *kbuf;
	size_t len;
	kbuf = (char*) kmalloc(sizeof(char)*PATH_MAX);

	// Maybe need to initialize stderr, in, and out.
	result = copyinstr((const_userptr_t) path, kbuf, PATH_MAX, &len);
	if(result) {
		kfree(kbuf);
		return result;
	}
	while (curthread->fdtable[index] != NULL ) {
		index++;
	}

	if(index == MAX_FILES) {
		kfree(kbuf);
		return ENFILE;
	}
	
	curthread->fdtable[index] = (struct file_info*) kmalloc(sizeof(struct file_info*));
	if(curthread->fdtable[index] == NULL) {
		kfree(kbuf);
		return ENFILE;
	}

	result = vfs_open(kbuf, mode, &vn);
	if(result) {
		kfree(kbuf);
		kfree(curthread->fdtable[index]);
		curthread->fdtable[index] = NULL;
		return result;
	}

	curthread->fdtable[index]->vn = vn;
	strcpy(curthread->fdtable[index]->file_name, kbuf);
	curthread->fdtable[index]->flags = oflag;
	curthread->fdtable[index]->refcount = 1;
	curthread->fdtable[index]->offset = 0;
	curthread->fdtable[index]->mutex = lock_create(kbuf);
	
	*fd = index;
	kfree(kbuf);
	return 0;
}
/*
#define DEFINE(type, x, body) { \
	int x { \
		body; \
	} \
}
	
DEFINE(
	int,
	sys_read(int fd, void* buf, size_t nbytes),
	{
		if (filehandle >= OPEN_MAX || filehandle < 0) {
			return EBADF;
		}
	}
);
*/

#define CHECK(x) if (x) { \
	return EBADF; \
}

int sys_read(int fd, void* buf, size_t nbytes) {
	int result = 0;

	CHECK(fd >= MAX_FILES || fd < 0)
	CHECK(curthread->fdtable[fd] == NULL) 
	CHECK(curthread->fdtable[fd]->flags == WRITE_ONLY)
	
	struct iovec iov;
	struct uio ku;
	void *kbuf;

	kbuf = kmalloc(sizeof(*buf)* nbytes);
	if(kbuf == NULL) {
		return EINVAL;
	}

	lock_acquire(curthread->fdtable[fd]->mutex);
	iov.iov_ubase = (userptr_t) buf;
	iov.iov_len = nbytes;
	ku.uio_iovec = iov;
	mk_kuio(&ku, kbuf, nbytes, curthread->fdtable[fd]->offset, UIO_READ);
	/*ku.uio_iovcnt = 1;
	ku.uio_offset = curthread->fdtable[fd]->offset;
	ku.uio_resid = nbytes;
	ku.uio_rw = UIO_READ;
	ku.uio_space = curthread->t_vmspace;
	*/
	result = VOP_READ(curthread->fdtable[fd]->vn, &ku);
	if(result) {
		kfree(kbuf);
		lock_release(curthread->fdtable[fd]->mutex);
		return result;
	}	

	kfree(kbuf);
	lock_release(curthread->fdtable[fd]->mutex);
	return 0;

}

int sys_write(int fd, const void* buf, size_t nbytes) {
	int result = 0;
	
	CHECK(fd >= MAX_FILES || fd < 0)
	CHECK(curthread->fdtable[fd] == NULL) 
	CHECK(curthread->fdtable[fd]->flags == READ_ONLY)
		
	struct iovec iov;
	struct uio ku;
	void *kbuf;

	kbuf = kmalloc(sizeof(*buf)* nbytes);
	if(kbuf == NULL) {
		return EINVAL;
	}
	lock_acquire(curthread->fdtable[fd]->mutex);
	
	result = copyin((const_userptr_t) buf, kbuf, nbytes);
	if(result) {
		kfree(kbuf);
		lock_release(curthread->fdtable[fd]->mutex);
		return result;
	}
	iov.iov_ubase = (userptr_t) buf;
	iov.iov_len = nbytes;
	ku.uio_iovec = iov;
	mk_kuio(&ku, kbuf, nbytes, curthread->fdtable[fd]->offset, UIO_WRITE);
	/* 
	ku.uio_iov = &iov;
	ku.uio_iovcnt = 1;
	ku.uio_offset = curthread->fdtable[fd]->offset;
	ku.uio_resid = nbytes;
	ku.uio_segflg = UIO_USERSPACE;
	ku.uio_rw = UIO_WRITE;
	ku.uio_space = curthread->t_vmspace;
	*/
	result = VOP_WRITE(curthread->fdtable[fd]->vn, &ku);
	if(result) {
		kfree(kbuf);
		lock_release(curthread->fdtable[fd]->mutex);
		return result;
	}	
	
	curthread->fdtable[fd]->offset = ku.uio_offset;

	kfree(kbuf);
	lock_release(curthread->fdtable[fd]->mutex);
	return 0;


}

int sys_lseek(int fd, off_t offset, int whence, int* ret) {
	
	int result = 0;
	if(fd >= MAX_FILES || fd < 0) {
		return EBADF;
	}
	
	if(curthread->fdtable[fd] == NULL) {
		return EBADF;
	}

	off_t pos, file_size;
	struct stat statbuf;

	lock_acquire(curthread->fdtable[fd]->mutex);
	result = VOP_STAT(curthread->fdtable[fd]->vn, &statbuf);
	if (result) {
		lock_release(curthread->fdtable[fd]->mutex);
		return result;
	}

	file_size = statbuf.st_size;
	
	if (whence == SEEK_SET) {
		result = VOP_TRYSEEK(curthread->fdtable[fd]->vn, offset);
		if(result) {
			lock_release(curthread->fdtable[fd]->mutex);
			return result;
		}
		pos = offset;
	} else if (whence == SEEK_END) {
		result = VOP_TRYSEEK(curthread->fdtable[fd]->vn, (file_size + pos));
		if(result) {
			lock_release(curthread->fdtable[fd]->mutex);
			return result;
		}
		pos = offset + pos;
	} else {
		lock_release(curthread->fdtable[fd]->mutex);
		return EINVAL;
	}

	if(offset < (off_t) 0) {
		lock_release(curthread->fdtable[fd]->mutex);
		return EINVAL;
	}

	curthread->fdtable[fd]->offset = pos;

	lock_release(curthread->fdtable[fd]->mutex);
	*ret = (unsigned int)(offset & 0xFFFFFFFF);
	return 0;
}	

int sys_close(int fd, int* ret) {
	CHECK(fd >= MAX_FILES || fd < 0)
	CHECK(curthread->fdtable[fd] == NULL)
	CHECK(curthread->fdtable[fd]->vn == NULL)

	if(curthread->fdtable[fd]->refcount == 1) {
		VOP_CLOSE(curthread->fdtable[fd]->vn);
		lock_destroy(curthread->fdtable[fd]->mutex);
		kfree(curthread->fdtable[fd]);
		curthread->fdtable[fd] = NULL;
	} else {
		curthread->fdtable[fd]->refcount -= 1;
	}
	*ret = 0;
	return 0;
}

int sys_dup2(int oldfd, int newfd, int *ret) {
	int result = 0;
	if(oldfd >= MAX_FILES || oldfd < 0 || newfd >= MAX_FILES || newfd < 0)  {
		return EBADF;
	}
	if(oldfd ==  newfd) {
		*ret = newfd;
		return 0;
	}
	if(curthread->fdtable[oldfd] == NULL) {
		return EBADF;
	}
	if(curthread->fdtable[oldfd] != NULL) {
		result = sys_close(newfd, ret);
		if(result) {
			return EBADF;
		}
	}
	else {
		curthread->fdtable[newfd] = (struct file_info*) kmalloc(sizeof(struct file_info*));
	}
	lock_acquire(curthread->fdtable[oldfd]->mutex);
	curthread->fdtable[newfd]->vn  = curthread->fdtable[oldfd]->vn;
	curthread->fdtable[newfd]->offset = curthread->fdtable[oldfd]->offset;
	curthread->fdtable[newfd]->flags = curthread->fdtable[oldfd]->flags;
	
	strcpy(curthread->fdtable[newfd]->file_name, curthread->fdtable[oldfd]->file_name);
	curthread->fdtable[newfd]->mutex = lock_create("duped file");

	lock_release(curthread->fdtable[oldfd]->mutex);
	*ret = newfd;
	return 0;
}


