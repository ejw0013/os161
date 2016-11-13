
#ifndef PROC_SYSCALL_H_
#define PROC_SYSCALL_H_

# include <kern/limits.h>
# include <types.h>
# include <../../arch/mips/include/trapframe.h>

struct process_table_entry {
	struct process* procs;
	pid_t pid;
	struct process_table_entry *next;
};

struct process {
	pid_t ppid;
	struct semaphore* exited;
	bool has_exited;
	int exitcode;
	struct thread* thread_process;
};

/* Maybe needs extern*/
struct process_table_entry *process_table;

/* Maybe needs extern*/
int process_count;

pid_t give_pid(void);
void init_process(struct thread *t, pid_t id);
void destroy_process(pid_t pid);
void entry_point(void* data1, unsigned long data2);
void change_ppid(pid_t change, pid_t ppid);

int sys_execv(const char* program, char **uargs); 
int sys_getpid(pid_t* retval); 
int sys_waitpid(pid_t pid, int *status, int options, int* retval);
int sys_fork(struct trapframe *tf, int* retval);
int sys__exit(int exitcode);

#endif
