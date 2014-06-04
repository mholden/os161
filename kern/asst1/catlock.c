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
#include <synch.h>
#include <thread.h>


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

static struct lock *mouse_lock;
static struct lock *cat_lock;
static struct lock *next_spot;
static struct lock *bowl;
static struct cv *cat_empty;
static struct cv *mouse_empty;
static struct cv *cat_not_full;
static struct cv *mouse_not_full;
static struct cv *cat_not_halfway;
static struct cv *mouse_not_halfway;
int bowl1, bowl2, mouse_count, cat_count, done;
/*
 * 
 * Function Definitions
 * 
 */

/* who should be "cat" or "mouse" */
static void
lock_eat(const char *who, int num, int bowl, int iteration)
{
        kprintf("%s: %d starts eating: bowl %d, iteration %d\n", who, num, 
                bowl, iteration);
        clocksleep(1);
        kprintf("%s: %d ends eating: bowl %d, iteration %d\n", who, num, 
                bowl, iteration);
}

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
	int bowl_used, iteration;
	for(iteration = 0; iteration < 4; iteration++){
	lock_acquire(next_spot);
	while(mouse_count == 1)
		cv_wait(mouse_not_halfway, next_spot);
	lock_acquire(mouse_lock);
	if (mouse_count != 0)
		cv_wait(mouse_empty, mouse_lock);
	lock_acquire(cat_lock);
    lock_release(mouse_lock);
    lock_release(next_spot);

    while(cat_count == 2)
    	cv_wait(cat_not_full, cat_lock);
	cat_count++;
	if(cat_count == 1)
		cv_broadcast(cat_not_full, cat_lock);
	if(cat_count == 2)
		cv_broadcast(cat_not_halfway, cat_lock);
	lock_acquire(bowl);
		if(bowl1){
			bowl2 = 1;
			bowl_used = 2;
		}
		else{
			bowl1 = 1;
			bowl_used = 1;
		}
	lock_release(cat_lock);
	lock_release(bowl);
	lock_eat("cat", catnumber, bowl_used, iteration);
	lock_acquire(cat_lock);
	lock_acquire(bowl);
	if (bowl_used == 1)
		bowl1 = 0;
	else
		bowl2 = 0;
	cat_count--;
	if(cat_count == 0){
		cv_broadcast(cat_empty, cat_lock);
		cv_broadcast(cat_not_halfway, cat_lock);
	}
	if(cat_count == 1)
		cv_broadcast(cat_not_full, cat_lock);
	lock_release(cat_lock);
	lock_release(bowl);
	}
	done++;
	(void) unusedpointer;
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
	int bowl_used, iteration;
		for(iteration = 0; iteration < 4; iteration++){
		lock_acquire(next_spot);
		while(cat_count == 1)
			cv_wait(cat_not_halfway, next_spot);
		lock_acquire(cat_lock);
		if (cat_count != 0)
			cv_wait(cat_empty, cat_lock);
		lock_acquire(mouse_lock);
	    lock_release(cat_lock);
	    lock_release(next_spot);

	    while(mouse_count == 2)
	    	cv_wait(mouse_not_full, mouse_lock);
		mouse_count++;
		if(mouse_count == 1)
			cv_broadcast(mouse_not_full, mouse_lock);
		if(mouse_count == 2)
			cv_broadcast(mouse_not_halfway, mouse_lock);
		lock_acquire(bowl);
			if(bowl1){
				bowl2 = 1;
				bowl_used = 2;
			}
			else{
				bowl1 = 1;
				bowl_used = 1;
			}
		lock_release(mouse_lock);
		lock_release(bowl);
		lock_eat("mouse", mousenumber, bowl_used, iteration);
		lock_acquire(mouse_lock);
		lock_acquire(bowl);
		if (bowl_used == 1)
			bowl1 = 0;
		else
			bowl2 = 0;
		mouse_count--;
		if(mouse_count == 0){
			cv_broadcast(mouse_empty, mouse_lock);
			cv_broadcast(mouse_not_halfway, mouse_lock);
		}
		if(cat_count == 1)
			cv_broadcast(mouse_not_full, mouse_lock);
		lock_release(mouse_lock);
		lock_release(bowl);
		}
		done++;
		(void) unusedpointer;
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
        /*
         * Avoid unused variable warnings.
         */

        (void) nargs;
        (void) args;
   
        /*
         * Locks creation
         */

        mouse_lock = lock_create("mouse_lock");
        cat_lock = lock_create("cat_lock");
        next_spot = lock_create("next_spot");
        bowl = lock_create("bowl");

        /*
         * CV creation
         */
        mouse_empty = cv_create("mouse_empty");
        cat_empty = cv_create("cat_empty");
        mouse_not_full = cv_create("mouse_not_full");
        cat_not_full = cv_create("cat_not_full");
        mouse_not_halfway = cv_create("mouse_not_halfway");
        cat_not_halfway = cv_create("cat_not_halfway");

        /*
         * Global variables
         */
        done = 0;
        bowl1 = 0;
        bowl2 = 0;
        mouse_count = 0;
        cat_count = 0;

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
        while(done < (NMICE + NCATS));

        lock_destroy(mouse_lock);
        lock_destroy(cat_lock);
        lock_destroy(next_spot);
        lock_destroy(bowl);

        /*
         * CV destruction
         */
        cv_destroy(mouse_empty);
        cv_destroy(cat_empty);
        cv_destroy(mouse_not_full);
        cv_destroy(cat_not_full);
        cv_destroy(mouse_not_halfway);
        cv_destroy(cat_not_halfway);

        return 0;
}

/*
 * End of catlock.c
 */
