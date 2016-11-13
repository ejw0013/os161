/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <thread.h>
#include <curthread.h>
#include <vm.h>
#include <vfs.h>
#include <test.h>
#include <kern/file_syscalls.h>
/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname, char** args)
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result, len;
	int index = 0;

	/*STD_IN/OUT/ERR are not intialized */
	if(curthread->fdtable[0] == NULL) {
		result = filetable_init();
		if(result) {
			return result;
		}
	}

	while(args[index] != NULL) {
		index++;
	}
	char** allargs = (char**) kmalloc(sizeof(char*) *index);
	index = 0;

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, &v);
	if (result) {
		return result;
	}

	/* We should be a new thread. */
	assert(curthread->t_vmspace == NULL);

	/* Create a new address space. */
	curthread->t_vmspace = as_create();
	if (curthread->t_vmspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Activate it. */
	as_activate(curthread->t_vmspace);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_vmspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		return result;
	}

	while (args[index] != NULL) {
		char* arg;
		len = strlen(args[index]) + 1;
		
		int original_len = len;
		if (len % 4 != 0) {
			len = len + (4 - len % 4);
		}

		arg = kmalloc(sizeof(len));
		arg = kstrdup(args[index]);
		int i;
		for (i = 0; i < len; i++) {
 			if (i > original_len) arg[i] = '\0';
			else arg[i] = args[index][i];

		}
		stackptr -= len;
		result = copyout((const void*) arg, (userptr_t)stackptr, (size_t) len);
		if (result) {
			kprintf("Copyout failed in runprogram %d\n", result);
			return result;
		}
		
		kfree(arg);
		allargs[index] = (char*) stackptr;
		index++;
	}
	
	if (args[index] == NULL) {
		stackptr -= 4 * sizeof(char);
	}
	
	int i;	
	for (i = (index - 1); i >= 0; i--) {
		stackptr = stackptr - sizeof(char*);
		result = copyout((const void*)(allargs + i), (userptr_t)stackptr, (sizeof(char*)));
		if (result) {
			kprintf("Copyout failed in rungram, result %d, arr index %d\n", result, i);
			return result;
		}
	}

	/* Warp to user mode. */
	md_usermode(0 /*argc*/, NULL /*userspace addr of argv*/,
		    stackptr, entrypoint);
	
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;
}

