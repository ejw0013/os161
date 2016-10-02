/*
 * catsem.c
 *
 * 30-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: Please use SEMAPHORES to solve the cat syncronization problem in 
 * this file.
 */


/*
 * 
 * Includes
 *
 */

#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>
#include <synch.h>
/*
 * 
 * Constants
 *
 */

/*
 * Number of food bowls.
 */

#define NFOODBOWLS 2

/*
 * Number of cats.
 */

#define NCATS 6

/*
 * Number of mice.
 */

#define NMICE 2

/*
 *
 * Global variables
 * 
 */

static volatile int num_cats_eating, num_mice_eating, cats_waiting, mice_waiting, turn, animals_full, cats_allowed, mice_allowed;
static struct semaphore *lobby, *done, *bowls, *cats_done, *mice_done; 
static struct lock *mutex, *m_lock, *c_lock;
/*
 * 
 * Function Definitions
 * 
 */


/*
 * catsem()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long catnumber: holds the cat identifier from 0 to NCATS - 1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Write and comment this function using semaphores.
 *
 */
static
void
catsem(void * unusedpointer, 
       unsigned long catnumber)
{	
	//c_lock keeps cats from skipping over the wait it's not their turn
	//kprintf("Cat %lu waiting for c_lock\n", catnumber);
	lock_acquire(c_lock);
	//kprintf("Cat %lu has c_lock\n", catnumber);
	lock_acquire(mutex);
	cats_waiting++;
	if (cats_waiting == 1) { //if we're the first cat
		cats_allowed = 2;
		lock_release(mutex);
		//kprintf("Cat %lu waiting outside kitchen\n", catnumber);
		P(mice_done);
		lock_acquire(mutex);
	} else{
		//new code
		if(cats_allowed == 0) {
			lock_release(mutex);
			//kprintf("Cat %lu waiting outside kitchen\n", catnumber);
			P(mice_done); //wait until mice are done with their turn
			lock_acquire(mutex);
		}
	}
	//kprintf("Cat %lu releasing c_lock\n", catnumber);
	kprintf("Cat %lu enters lobby\n", catnumber);
	lock_release(c_lock);
	cats_allowed--;
	/* Eating */
	lock_release(mutex);
	P(bowls);
	kprintf("Cat %lu enters kitchen and is eating\n", catnumber);
	clocksleep(1);
	kprintf("Cat %lu is done eating and leaves kitchen\n", catnumber, catnumber);
	V(bowls);
	lock_acquire(mutex);
	animals_full++;
	cats_waiting--;
	//last cat in the kitchen
	//move full check priority up. Also functions as a psuedo "if none waiting" check.
	if (animals_full == NMICE + NCATS) {
		lock_release(mutex);
		V(done);
	} else {
		if (cats_waiting == 0 && mice_waiting != 0) {
			lock_release(mutex);
			V(cats_done);
		} else if ( cats_waiting != 0 && mice_waiting == 0) {
			cats_allowed = 2;
			lock_release(mutex);
			V(mice_done);
		} else {
			//at least 1 of each waiting. Let another cat in to fulfill quota
			lock_release(mutex);
			V(cats_done);
		}
	}
}
        
/*
 * mousesem()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long mousenumber: holds the mouse identifier from 0 to 
 *              NMICE - 1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Write and comment this function using semaphores.
 *
 */

static
void
mousesem(void * unusedpointer, 
         unsigned long mousenumber)
{
	//m_lock keeps mice from skipping over the wait if it's not their turn
	//kprintf("Mouse %lu waiting for m_lock\n", mousenumber);
	lock_acquire(m_lock);
	//kprintf("Mouse %lu has m_lock\n", mousenumber);
	lock_acquire(mutex);
	mice_waiting++;
	if (mice_waiting == 1) {
		mice_allowed = 2;
		lock_release(mutex);
		//kprintf("Mouse %lu waiting outside kitchen\n", mousenumber);
		P(cats_done);
		lock_acquire(mutex);
	} else {
		if(mice_allowed == 0){
			lock_release(mutex);
			//kprintf("Mouse %lu waiting outside kitchen\n", mousenumber);
			P(cats_done);
			lock_acquire(mutex);
		}
	}
	//kprintf("Mouse %lu releasing m_lock\n", mousenumber);
	kprintf("Mouse %lu enters lobby\n", mousenumber);
	lock_release(m_lock);
	mice_allowed--;
	/* Eating */
	lock_release(mutex);
	P(bowls);
	kprintf("Mouse %lu enters kitchen and is eating\n", mousenumber);
	clocksleep(1);
	kprintf("Mouse %lu is done eating and leaves kitchen\n", mousenumber, mousenumber);
	V(bowls);
	lock_acquire(mutex);
	animals_full++;
	mice_waiting--;
	//last mouse
	//moved full check priority up. Also functions as a psuedo "if none waiting" check.
	if (animals_full == NMICE + NCATS) {
		lock_release(mutex);
		V(done);
	} else {
		if (mice_waiting == 0 && cats_waiting != 0) {
			lock_release(mutex);
			V(mice_done);
		} else if(mice_waiting != 0 && cats_waiting == 0) {
			mice_allowed = 2;
			lock_release(mutex);
			V(cats_done);
		} else { 
			//at least 1 of each waiting. Let another mouse in to fulfill quota.
			lock_release(mutex);
			V(cats_done);
		}
	}
}

/*
 * catmousesem()
 *
 * Arguments:
 *      int nargs: unused.
 *      char ** args: unused.
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up catsem() and mousesem() threads.  Change this 
 *      code as necessary for your solution.
 */

int
catmousesem(int nargs,
            char ** args)
{
        int index, error;
   
	cats_allowed = 2;
	cats_waiting = 0;
	cats_waiting = 0;
	mice_waiting = 0;
	animals_full = 0;
	done = sem_create("done", 0);
	bowls = sem_create("bowls", NFOODBOWLS);
	cats_done = sem_create("cats_done", 0);
	mice_done = sem_create("mice_done", 1);
	mutex = lock_create("mutex");
	m_lock = lock_create("m_lock");
	c_lock = lock_create("c_lock");
        /*
         * Avoid unused variable warnings.
         */

        (void) nargs;
        (void) args;
   
        /*
         * Start NCATS catsem() threads.
         */

        for (index = 0; index < NCATS; index++) {
           
                error = thread_fork("catsem Thread", 
                                    NULL, 
                                    index, 
                                    catsem, 
                                    NULL
                                    );
                
                /*
                 * panic() on error.
                 */

                if (error) {
                 
                        panic("catsem: thread_fork failed: %s\n", 
                              strerror(error)
                              );
                }
        }
        
        /*
         * Start NMICE mousesem() threads.
         */

        for (index = 0; index < NMICE; index++) {
   
                error = thread_fork("mousesem Thread", 
                                    NULL, 
                                    index, 
                                    mousesem, 
                                    NULL
                                    );
                
                /*
                 * panic() on error.
                 */

                if (error) {
         
                        panic("mousesem: thread_fork failed: %s\n", 
                              strerror(error)
                              );
                }
        }
	P(done);
        return 0;
}


/*
 * End of catsem.c
 */
