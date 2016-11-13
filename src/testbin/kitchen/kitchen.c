/*
 * kitchen.c
 *
 * 	Run a bunch of sinks (only).
 *
 * This tests concurrent read access to the console driver.
 */

#include <unistd.h>
#include <err.h>
#include <stdio.h>
static char *sargv[2] = { (char *)"sink", NULL };

#define MAXPROCS  6
static int pids[MAXPROCS], npids;

static
void
sink(void)
{
	printf("asdf\n");
	int pid = fork();
	switch (pid) {
	    case -1:
		err(1, "fork");
		printf("-1\n");
	    case 0:
		/* child */
		execv("/testbin/sink", sargv);
		err(1, "/testbin/sink");
		printf("0\n");
	    default:
		/* parent */
		pids[npids++] = pid;
		printf("default\n");
		break;
	}
}

static
void
waitall(void)
{
	int i, status;
	for (i=0; i<npids; i++) {
		if (waitpid(pids[i], &status, 0)<0) {
			warn("waitpid for %d", pids[i]);
		}
		else if (status != 0) {
			warnx("pid %d: exit %d", pids[i], status);
		}
	}
}


int
main(void)
{
	printf("here\n");
	sink();
	sink();
	sink();
	sink();

	waitall();

	return 0;
}
