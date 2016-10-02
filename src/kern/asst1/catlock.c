/*
 * catlock.c
 *
 * 30-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: Please use LOCKS/CV'S to solve the cat syncronization problem in 
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
 * Function Definitions
 * 
 */

#define NOCATMOUSE 0
#define CATS 1
#define MICE 2
#define boolean int
#define true 0
#define false 1

static volatile int cats_wait_count, mice_wait_count, cats_eating, mice_eating, animals_full;
static volatile boolean dish1_busy, dish2_busy;
static volatile int cats_turn, mice_turn;
static struct lock* mutex;
static struct cv *turn_cv, *done_cv;
static volatile int turn_type;


/*
 * catlock()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long catnumber: holds the cat identifier from 0 to NCATS -
 *      1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Write and comment this function using locks/cv's.
 *
 */

static
void
catlock(void * unusedpointer, 
        unsigned long catnumber)
{
	(void) unusedpointer;
	int mydish = -1;
	lock_acquire(mutex); //lock shared variables
	cats_wait_count++;

	if (turn_type == NOCATMOUSE) {
		//if I'm the first animal, then claim turn and setup cats this turn
		turn_type = CATS;
		cats_turn = 2;
	} else {
		//otherwise wait until it's the cats turn
		while (cats_turn == 0 || turn_type != CATS) {
			cv_wait(turn_cv, mutex); // TODO: Could be here.
		}	
	}
	cats_eating++; //increase the cats currently eating
	cats_turn--; // decrease the cats allowed in for the rest of this turn
	kprintf("Cat %lu enters the kitchen\n", catnumber);

	if (dish1_busy == false) {
		dish1_busy = true;
		mydish = 1;
	} else if (dish2_busy == false ) {
		dish2_busy = true;
		mydish = 2;
	} else {
		panic("Cat %lu can't find an empty dish\n", catnumber);
	}
	kprintf("Cat %lu is eating at bowl %d\n", catnumber, mydish);
	lock_release(mutex);
	clocksleep(1);
	lock_acquire(mutex);
	
	kprintf("Cat %lu finishes eating at bowl %d\n", catnumber, mydish);
	
	if (mydish == 1) {
		dish1_busy = false;
	} else {
		dish2_busy = false;
	}

	cats_eating--;
	cats_wait_count--;
	
	/*
 	 * 
 	 * As the last cat in the kitchen there are three cases
 	 * Case 1: There are no animals waiting at all, signal the done_cv
 	 * Case 2: There are no waiting mice, but waiting cats, signal sleeping cats.
	 * Case 3: There are waiting cats and but waiting mice, signal sleeping mice.
	 *
	 */
	
	if (cats_eating == 0) { //if I'm the last cat
		if ( cats_wait_count == 0 && mice_wait_count == 0 ) { //Case 1
			cv_signal(done_cv, mutex);
		} else if (mice_wait_count != 0) { //Case 3
			turn_type = MICE;
			mice_turn = 2;
			kprintf("Cat %lu is giving kitchen to mice.\n", catnumber);
			cv_broadcast(turn_cv, mutex);
		} else if ( mice_wait_count == 0 && cats_wait_count != 0) { //Case 2
			cats_turn = 2;
			cv_broadcast(turn_cv, mutex); //wake up another cat to enter;
		} else {
			panic("There was a misisng case in our CATS switch turn cases. Rip.\n");
		}
	}
	animals_full++;
	lock_release(mutex);
}
	

/*
 * mouselock()
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
 *      Write and comment this function using locks/cv's.
 *
 */

static
void
mouselock(void * unusedpointer,
          unsigned long mousenumber)
{
	(void) unusedpointer;

    	int mydish = -1;
	lock_acquire(mutex); //lock shared variables
	mice_wait_count++;

	if (turn_type == NOCATMOUSE) {
		//if I'm the first animal, then claim turn and setup cats this turn
		turn_type = MICE;
		mice_turn = 2;
	} else {
		//otherwise wait until it's the cats turn
		while (mice_turn == 0 || turn_type != MICE) {
			cv_wait(turn_cv, mutex);
		}	
	}
	mice_eating++; //increase the cats currently eating
	mice_turn--; // decrease the cats allowed in for the rest of this turn
	kprintf("Mouse %lu enters the kitchen\n", mousenumber);

	if (dish1_busy == false) {
		dish1_busy = true;
		mydish = 1;
	} else if (dish2_busy == false ) {
		dish2_busy = true;
		mydish = 2;
	} else {
		panic("Mouse %lu can't find an empty dish\n", mousenumber);
	}
	kprintf("Mouse %lu is eating at bowl %d\n", mousenumber, mydish);
	lock_release(mutex);
	clocksleep(1);
	lock_acquire(mutex);
	
	kprintf("Mouse %lu finishes eating at bowl %d\n", mousenumber, mydish);
	
	if (mydish == 1) {
		dish1_busy = false;
	} else {
		dish2_busy = false;
	}

	mice_eating--;
	mice_wait_count--;
	
	/*
 	 * 
 	 * As the last cat in the kitchen there are three cases
 	 * Case 1: There are no animals waiting at all, signal the done_cv
 	 * Case 2: There are no waiting mice, but waiting cats, signal sleeping cats.
	 * Case 3: There are waiting cats and but waiting mice, signal sleeping mice.
	 *
	 */
	
	if (mice_eating == 0) { //if I'm the last cat
		if ( cats_wait_count == 0 && mice_wait_count == 0 ) { //Case 1
			cv_signal(done_cv, mutex);
		} else if (cats_wait_count != 0) { //Case 3
			turn_type = CATS;
			cats_turn = 2;
			kprintf("Mouse %lu is giving kitchen to cats.\n", mousenumber);
			cv_broadcast(turn_cv, mutex);
		} else if ( cats_wait_count == 0 && mice_wait_count != 0) { //Case 2
			mice_turn = 2;
			cv_broadcast(turn_cv, mutex); //wake up another cat to enter;
		} else {
			panic("There was a misisng case in our MICE or general switch turn cases. Rip.\n");
		}
	}
	animals_full++;
	lock_release(mutex);

        
}


/*
 * catmouselock()
 *
 * Arguments:
 *      int nargs: unused.
 *      char ** args: unused.
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up catlock() and mouselock() threads.  Change
 *      this code as necessary for your solution.
 */

int
catmouselock(int nargs,
             char ** args)
{
        int index, error;
	animals_full = 0;
	dish1_busy = false;
	dish2_busy = false;
	cats_wait_count = 0;
	mice_wait_count = 0;
	
	cats_turn = 2;
	mice_turn = 0;

	cats_eating = 0;
	mice_eating = 0;

	turn_type = NOCATMOUSE;
	
	mutex = lock_create("catlock mutex");
	turn_cv = cv_create("catlock turn cv");
	done_cv = cv_create("catlock done cv");
   
        /*
         * Avoid unused variable warnings.
         */

        (void) nargs;
        (void) args;
   
        /*
         * Start NCATS catlock() threads.
         */

        for (index = 0; index < NCATS; index++) {
           
                error = thread_fork("catlock thread", 
                                    NULL, 
                                    index, 
                                    catlock, 
                                    NULL
                                    );
                
                /*
                 * panic() on error.
                 */

                if (error) {
                 
                        panic("catlock: thread_fork failed: %s\n", 
                              strerror(error)
                              );
                }
        }

        /*
         * Start NMICE mouselock() threads.
         */

        for (index = 0; index < NMICE; index++) {
   
                error = thread_fork("mouselock thread", 
                                    NULL, 
                                    index, 
                                    mouselock, 
                                    NULL
                                    );
      
                /*
                 * panic() on error.
                 */

                if (error) {
         
                        panic("mouselock: thread_fork failed: %s\n", 
                              strerror(error)
                              );
                }
        }
	//wait until signaled done.
	lock_acquire(mutex);
	while (animals_full != NCATS + NMICE) {
		cv_wait(done_cv, mutex);
	}
	lock_release(mutex);
        return 0;
}

/*
 * End of catlock.c
 */
