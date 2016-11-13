#include <kern/proc_syscalls.h>
#include <thread.h>
#include <types.h>
#include <curthread.h>

#include <kern/errno.h>
#include <../include/synch.h>

#include <lib.h>

#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <../arch/mips/include/spl.h>
#include <kern/file_syscalls.h>
#include <machine/spl.h>
struct process_table_entry* process_table = NULL;

int process_count;

void func(void* data1, unsigned long data2) {
	struct trapframe* tf, tfnew;
	struct addrspace* addr;
	
	tf = (struct trapframe*) data1;
	addr = (struct addrspace*) data2;

	tf->tf_a3 = 0;
	tf->tf_v0 = 0;
	tf->tf_epc += 4;

	curthread->t_vmspace = addr;
	as_activate(addr);

	tfnew = *tf;
	mips_usermode(&tfnew);
}



pid_t give_pid(void) {
	/* If the table is NULL, then we know this is the first process*/
	if (process_table == NULL) {
		//Initialize process pid and global pid count to one and allocate memory for the process table
		process_count = 1;
		process_table = (struct process_table_entry*) kmalloc(sizeof(struct process_table_entry));
		process_table->next = NULL;
		process_table->pid = 1;
		process_table->procs = (struct process*) kmalloc(sizeof(struct process));

	}
	//kprintf("Giving pid %s\n", process_count + 1);
	return ++process_count;

}

void init_process(struct thread *t, pid_t id) {
	//Create a temporary process pointer to intialize memory
	struct process* temp;
	temp = (struct process*) kmalloc(sizeof(struct process));
	
	temp->ppid = t->ppid;
	temp->has_exited = false;
	temp->exitcode = 0;
	temp->thread_process = t;
	struct semaphore* sem;
	sem = sem_create("process for child", 0);
	temp->exited = sem;
	struct process_table_entry* temp1;
	
	for (temp1 = process_table; temp1->next!=NULL; temp1=temp1->next);
	temp1->next = (struct process_table_entry*) kmalloc(sizeof(struct process_table_entry));
	temp1->next->next = NULL;
	temp1->next->procs = temp;
	temp1->next->pid = id;
}

int sys_getpid(pid_t* retval) {
	*retval = curthread->pid;
	return 0;
}

int sys_fork(struct trapframe *tf, pid_t* retval) {
	int result = 0;
	struct thread* ch_thread;
	
	struct addrspace* child_addrspace = kmalloc(sizeof(struct addrspace));
	result = as_copy(curthread->t_vmspace, &child_addrspace);
	if (result) {
		return ENOMEM;
	} 
	
	struct trapframe* tf_child = kmalloc(sizeof(struct trapframe));
	if (tf_child == NULL) {
		return ENOMEM;
	}
	*tf_child = *tf;
	result = thread_fork("Child Thread", (struct trapframe*) tf_child,
			    (unsigned long) child_addrspace, func, &ch_thread);
	if(result) {
		return ENOMEM;
	} 

	*retval = ch_thread->pid;
	
	return 0;
}

int sys_waitpid(pid_t pid, int* status, int options, int *retval) {
	int result;
	struct process *child;

	if (options != 0) {
		return EINVAL;
	}

	if (status == NULL) {
		return EFAULT;
	}

	if(pid < PID_MIN) {
		return EINVAL;
	}

	if (pid > PID_MAX) {
		return EINVAL;
	}

	if (pid == curthread->pid) {
		return EINVAL;
	}

	if (pid == curthread->ppid) {
		return EINVAL;
	}

	struct process_table_entry* temp1;
	for (temp1 = process_table; temp1->pid != pid && temp1->next != NULL; temp1 = temp1->next);

	if (temp1 == NULL) {
		return EINVAL;
	}
	
	if (temp1->procs->ppid != curthread->pid) {
		return EINVAL;
	}	

	if (temp1->procs->has_exited == false) {
		P(temp1->procs->exited);
	}

	child = temp1->procs;
	
	result = copyout((const void *) &(child->exitcode), (userptr_t) status, sizeof(int));
	
	if (result) {
		return EFAULT;
	}
	
	*retval = pid;

	destroy_process(pid);
	return 0;

}

void changeppid(pid_t change, pid_t ppid) {
	struct process_table_entry* temp1;
	for (temp1 = process_table; temp1->pid != change; temp1 = temp1->next);
	temp1->procs->ppid = ppid;
}

int sys_execv(const char* program, char **uargs) {
	struct vnode *v;
	vaddr_t func, stackptr;
	int result, len;
	int index = 0;

	int i = 0;
	lock_acquire(execv_lock);
	if (program == NULL || uargs == NULL ) {
		return EFAULT;
	}

	char *progname;
	size_t size;
	progname = (char*) kmalloc(sizeof(char)* PATH_MAX);
	result = copyinstr((const_userptr_t) program, progname, PATH_MAX, &size);
	if (result) {
		kfree(progname);
		return EFAULT;
	}
	if(size == 1) {
		kfree(progname);
		return EINVAL;
	}
	char** args = (char**) kmalloc(sizeof(char **));
	result = copyin((const_userptr_t) uargs, args, sizeof(char**));
	if(result) {
		kfree(progname);
		kfree(args);
		return EFAULT;
	}
	while(uargs[i] != NULL ) {
		args[i] = (char*) kmalloc(sizeof(char)* PATH_MAX);
		result = copyinstr((const_userptr_t) uargs[i], args[i], PATH_MAX,
			&size);
		if(result) {
			kfree(progname);
			kfree(args);
			return EFAULT;
		}
		i++;
	}
	args[i] = NULL;
	
	result = vfs_open(progname, READ_ONLY, &v);
	if(result) {
		kfree(progname);
		kfree(args);
		return result;
	}
	
	struct addrspace *temp;
	temp = curthread->t_vmspace;

	if (curthread->t_vmspace != NULL) {
		as_destroy(curthread->t_vmspace);
		curthread->t_vmspace = NULL;
	}

	assert(curthread->t_vmspace == NULL);


	if ((curthread->t_vmspace = as_create()) == NULL ) {
		kfree(progname);
		kfree(args);
		vfs_close(v);
		return result;	
	}

	vfs_close(v);

	result = as_define_stack(curthread->t_vmspace, &stackptr);
	
	if(result) {
		kfree(progname);
		kfree(args);
		return result;
	}

	while (args[index] != NULL) {
		char* arg;
		len = strlen(args[index]) + 1;
		int oglen = len;

		if(len % 4 != 0) {
			len = len + (4 - len % 4);
		}
		
	

		arg = kmalloc(sizeof(len));
		arg = kstrdup(args[index]);
	
		for (i = 0; i < len; i++) {
		
			if (i >= oglen) {
				arg[i] = '\0';
			} else {
				arg[i] = args[index][i];
			}
		}

		stackptr -=len;

		result = copyout((const void*) arg, (userptr_t) stackptr,
				(size_t) len);
	
		if(result) {
			kfree(progname);
			kfree(args);
			kfree(arg);
			return result;
		}
		
		kfree(arg);
		args[index] = (char*) stackptr;

		index++;
	}

	if (args[index] == NULL) {
		stackptr -= 4 * sizeof(char);
	}
	for(i = (index - 1); i >= 0; i--) {
		stackptr = stackptr - sizeof(char*);
		result = copyout((const void*) (args + i), (userptr_t) stackptr,
			(sizeof(char *)));
		if (result) {
			kfree(progname);
			kfree(args);
			return result;
		}
	}

	kfree(progname);
	kfree(args);

	lock_release(execv_lock);
	/* Warp to user mode. */
	md_usermode(0 /*argc*/, NULL /*userspace addr of argv*/,
		    stackptr, func);
	
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;
}

int sys__exit(int exitcode) {
	
	struct process_table_entry* temp1;
	for (temp1 = process_table; temp1->pid != curthread->ppid || temp1 == NULL; temp1 = temp1->next);

	if (temp1 == NULL) {

	} else if (temp1->procs->exited == false) {
		
		struct process_table_entry* temp2;
		for (temp2 = process_table; temp2->pid != curthread->pid || temp2 == NULL; temp2 = temp2->next);
		if (temp2 == NULL) {
			kprintf("Process with PID %d not present in process table to exit/n", curthread->pid);
		} 
		temp2->procs->exitcode = exitcode;
		
		temp2->procs->has_exited = true;
		V(temp2->procs->exited);
	} else {
		destroy_process(curthread->pid);
	}

	thread_exit();
	return exitcode;
}

void destroy_process(pid_t pid) {
	struct process_table_entry* temp1;
	struct process_table_entry* temp2;
	for (temp1 = process_table; temp1->next->pid != pid; temp1 = temp1->next);
	temp2 = temp1->next;
	temp1->next = temp1->next->next;
	kfree(temp2->procs);
	kfree(temp2);
}

