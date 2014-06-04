/* 
 * stoplight.c
 *
 * 31-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: You can use any synchronization primitives available to solve
 * the stoplight problem in this file.
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
 * Static and global variables.
 */

#define NCARS 20

static struct semaphore *spots_avail_sem = NULL;
static struct semaphore *NE_block_sem = NULL;
static struct semaphore *NW_block_sem = NULL;
static struct semaphore *SE_block_sem = NULL;
static struct semaphore *SW_block_sem = NULL;
static struct semaphore *N_next_sem = NULL;
static struct semaphore *E_next_sem = NULL;
static struct semaphore *S_next_sem = NULL;
static struct semaphore *W_next_sem = NULL;
static struct semaphore *done_sem = NULL;


/*
 *
 * Function Definitions
 *
 */

static
void
inititems(void)
{
	if (spots_avail_sem==NULL) {
		spots_avail_sem = sem_create("spots_avail_sem", 3);
		if (spots_avail_sem == NULL) {
			panic("stoplight.c: sem_create failed\n");
		}
	}
	if (NE_block_sem==NULL) {
		NE_block_sem = sem_create("NE_block_sem", 1);
		if (NE_block_sem == NULL) {
			panic("stoplight.c: sem_create failed\n");
		}
	}
	if (NW_block_sem==NULL) {
		NW_block_sem = sem_create("NW_block_sem", 1);
		if (NW_block_sem == NULL) {
			panic("stoplight.c: sem_create failed\n");
		}
	}
	if (SE_block_sem==NULL) {
		SE_block_sem = sem_create("SE_block_sem", 1);
		if (SE_block_sem == NULL) {
			panic("stoplight.c: sem_create failed\n");
		}
	}
	if (SW_block_sem==NULL) {
		SW_block_sem = sem_create("SW_block_sem", 1);
		if (SW_block_sem == NULL) {
			panic("stoplight.c: sem_create failed\n");
		}
	}
	if (N_next_sem==NULL) {
		N_next_sem = sem_create("N_next_sem", 1);
		if (N_next_sem == NULL) {
			panic("stoplight.c: sem_create failed\n");
		}
	}
	if (E_next_sem==NULL) {
		E_next_sem = sem_create("E_next_sem", 1);
		if (E_next_sem == NULL) {
			panic("stoplight.c: sem_create failed\n");
		}
	}
	if (S_next_sem==NULL) {
		S_next_sem = sem_create("S_next_sem", 1);
		if (S_next_sem == NULL) {
			panic("stoplight.c: sem_create failed\n");
		}
	}
	if (W_next_sem==NULL) {
		W_next_sem = sem_create("W_next_sem", 1);
		if (W_next_sem == NULL) {
			panic("stoplight.c: sem_create failed\n");
		}
	}
	if (done_sem==NULL) {
		done_sem = sem_create("done_sem", 0);
		if (done_sem == NULL) {
			panic("stoplight.c: sem_create failed\n");
		}
	}
}


static const char *directions[] = { "N", "E", "S", "W" };

static const char *msgs[] = {
        "approaching:",
        "region1:    ",
        "region2:    ",
        "region3:    ",
        "leaving:    "
};

/* constants */
enum { APPROACHING, REGION1, REGION2, REGION3, LEAVING }; // for message()
enum { NORTH, EAST, SOUTH, WEST }; // approach directions
enum { RIGHT, STRAIGHT, LEFT }; // turn directions

static void
message(int msg_nr, int carnumber, int cardirection, int destdirection)
{
        kprintf("%s car = %2d, direction = %s, destination = %s\n",
                msgs[msg_nr], carnumber,
                directions[cardirection], directions[destdirection]);
}
 
/*
 * gostraight()
 *
 * Arguments:
 *      unsigned long cardirection: the direction from which the car
 *              approaches the intersection.
 *      unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement passing straight through the
 *      intersection from any direction.
 *      Write and comment this function.
 */

static
void
gostraight(unsigned long cardirection,
           unsigned long carnumber, 
	   unsigned long destdirection)
{
        switch(cardirection){
		case NORTH: 
			P(SW_block_sem);
			message(REGION2, carnumber, cardirection, destdirection);
			V(NW_block_sem);
			message(LEAVING, carnumber, cardirection, destdirection);
			V(SW_block_sem);
			break;
		case EAST: 
			P(NW_block_sem);
			message(REGION2, carnumber, cardirection, destdirection);
			V(NE_block_sem);
			message(LEAVING, carnumber, cardirection, destdirection);
			V(NW_block_sem);
			break;
		case SOUTH: 
			P(NE_block_sem);
			message(REGION2, carnumber, cardirection, destdirection);
			V(SE_block_sem);
			message(LEAVING, carnumber, cardirection, destdirection);
			V(NE_block_sem);
			break;
		case WEST: 
			P(SE_block_sem);
			message(REGION2, carnumber, cardirection, destdirection);
			V(SW_block_sem);
			message(LEAVING, carnumber, cardirection, destdirection);
			V(SE_block_sem);
			break;
	}
	V(spots_avail_sem);
}


/*
 * turnleft()
 *
 * Arguments:
 *      unsigned long cardirection: the direction from which the car
 *              approaches the intersection.
 *      unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement making a left turn through the 
 *      intersection from any direction.
 *      Write and comment this function.
 */

static
void
turnleft(unsigned long cardirection,
         unsigned long carnumber, 
	 unsigned long destdirection)
{
        switch(cardirection){
		case NORTH: 
			P(SW_block_sem);
			message(REGION2, carnumber, cardirection, destdirection);
			V(NW_block_sem);
			P(SE_block_sem);
			message(REGION3, carnumber, cardirection, destdirection);
			V(SW_block_sem);
			message(LEAVING, carnumber, cardirection, destdirection);
			V(SE_block_sem);
			break;
		case EAST: 
			P(NW_block_sem);
			message(REGION2, carnumber, cardirection, destdirection);
			V(NE_block_sem);
			P(SW_block_sem);
			message(REGION3, carnumber, cardirection, destdirection);
			V(NW_block_sem);
			message(LEAVING, carnumber, cardirection, destdirection);
			V(SW_block_sem);
			break;
		case SOUTH: 
			P(NE_block_sem);
			message(REGION2, carnumber, cardirection, destdirection);
			V(SE_block_sem);
			P(NW_block_sem);
			message(REGION3, carnumber, cardirection, destdirection);
			V(NE_block_sem);
			message(LEAVING, carnumber, cardirection, destdirection);
			V(NW_block_sem);
			break;
		case WEST: 
			P(SE_block_sem);
			message(REGION2, carnumber, cardirection, destdirection);
			V(SW_block_sem);
			P(NE_block_sem);
			message(REGION3, carnumber, cardirection, destdirection);
			V(SE_block_sem);
			message(LEAVING, carnumber, cardirection, destdirection);
			V(NE_block_sem);
			break;
	}
	V(spots_avail_sem);
}


/*
 * turnright()
 *
 * Arguments:
 *      unsigned long cardirection: the direction from which the car
 *              approaches the intersection.
 *      unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement making a right turn through the 
 *      intersection from any direction.
 *      Write and comment this function.
 */

static
void
turnright(unsigned long cardirection,
          unsigned long carnumber, 
	  unsigned long destdirection)
{
	switch(cardirection){
		case NORTH: 
			message(LEAVING, carnumber, cardirection, destdirection);
			V(NW_block_sem);
			break;
		case EAST: 
			message(LEAVING, carnumber, cardirection, destdirection);
			V(NE_block_sem);
			break;
		case SOUTH: 
			message(LEAVING, carnumber, cardirection, destdirection);
			V(SE_block_sem);
			break;
		case WEST: 
			message(LEAVING, carnumber, cardirection, destdirection);
			V(SW_block_sem);
			break;
	}
	V(spots_avail_sem);
}


/*
 * approachintersection()
 *
 * Arguments: 
 *      void * unusedpointer: currently unused.
 *      unsigned long carnumber: holds car id number.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Change this function as necessary to implement your solution. These
 *      threads are created by createcars().  Each one must choose a direction
 *      randomly, approach the intersection, choose a turn randomly, and then
 *      complete that turn.  The code to choose a direction randomly is
 *      provided, the rest is left to you to implement.  Making a turn
 *      or going straight should be done by calling one of the functions
 *      above.
 */
 
static
void
approachintersection(void * unusedpointer,
                     unsigned long carnumber)
{
        int cardirection;
	int turndirection;
	int destdirection;
	
	//Avoid unused variable warning
        (void) unusedpointer;
	
	//Determine the approach direction and turn direction randomly
        cardirection = random() % 4;
	turndirection = random() % 3;

	switch(cardirection){
		case NORTH: 
			switch(turndirection){
				case RIGHT: 
					destdirection = WEST;
					break;
				case STRAIGHT: 
					destdirection = SOUTH;
					break;
				case LEFT: 
					destdirection = EAST;
					break;
			}
			P(N_next_sem);
			message(APPROACHING, carnumber, cardirection, destdirection); //approaching
			P(spots_avail_sem);
			P(NW_block_sem);
			V(N_next_sem);
			message(REGION1, carnumber, cardirection, destdirection); //in region 1
			switch(turndirection){
				case RIGHT: 
					turnright(cardirection, carnumber, destdirection);
					break;
				case STRAIGHT: 
					gostraight(cardirection, carnumber, destdirection);
					break;
				case LEFT: 
					turnleft(cardirection, carnumber, destdirection);
					break;
			}
			break;
		case EAST: 
			switch(turndirection){
				case RIGHT: 
					destdirection = NORTH;
					break;
				case STRAIGHT: 
					destdirection = WEST;
					break;
				case LEFT: 
					destdirection = SOUTH;
					break;
			}
			P(E_next_sem);
			message(APPROACHING, carnumber, cardirection, destdirection); //approaching
			P(spots_avail_sem);
			P(NE_block_sem);
			V(E_next_sem);
			message(REGION1, carnumber, cardirection, destdirection); //in region 1
			switch(turndirection){
				case RIGHT: 
					turnright(cardirection, carnumber, destdirection);
					break;
				case STRAIGHT: 
					gostraight(cardirection, carnumber, destdirection);
					break;
				case LEFT: 
					turnleft(cardirection, carnumber, destdirection);
					break;
			}
			break;
		case SOUTH: 
			switch(turndirection){
				case RIGHT: 
					destdirection = EAST;
					break;
				case STRAIGHT: 
					destdirection = NORTH;
					break;
				case LEFT: 
					destdirection = WEST;
					break;
			}
			P(S_next_sem);
			message(APPROACHING, carnumber, cardirection, destdirection); //approaching
			P(spots_avail_sem);
			P(SE_block_sem);
			V(S_next_sem);
			message(REGION1, carnumber, cardirection, destdirection); //in region 1
			switch(turndirection){
				case RIGHT: 
					turnright(cardirection, carnumber, destdirection);
					break;
				case STRAIGHT: 
					gostraight(cardirection, carnumber, destdirection);
					break;
				case LEFT: 
					turnleft(cardirection, carnumber, destdirection);
					break;
			}
			break;
		case WEST: 
			switch(turndirection){
				case RIGHT: 
					destdirection = SOUTH;
					break;
				case STRAIGHT: 
					destdirection = EAST;
					break;
				case LEFT: 
					destdirection = NORTH;
					break;
			}
			P(W_next_sem);
			message(APPROACHING, carnumber, cardirection, destdirection); //approaching
			P(spots_avail_sem);
			P(SW_block_sem);
			V(W_next_sem);
			message(REGION1, carnumber, cardirection, destdirection); //in region 1
			switch(turndirection){
				case RIGHT: 
					turnright(cardirection, carnumber, destdirection);
					break;
				case STRAIGHT: 
					gostraight(cardirection, carnumber, destdirection);
					break;
				case LEFT: 
					turnleft(cardirection, carnumber, destdirection);
					break;
			}
			break;
	}

	V(done_sem);
}


/*
 * createcars()
 *
 * Arguments:
 *      int nargs: unused.
 *      char ** args: unused.
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up the approachintersection() threads.  You are
 *      free to modiy this code as necessary for your solution.
 */

int
createcars(int nargs,
           char ** args)
{
        int index, i, error;

        /*
         * Avoid unused variable warnings.
         */

        (void) nargs;
        (void) args;

	inititems();
	kprintf("Creating cars...\n");

        /*
         * Start NCARS approachintersection() threads.
         */

        for (index = 0; index < NCARS; index++) {

                error = thread_fork("approachintersection thread",
                                    NULL,
                                    index,
                                    approachintersection,
                                    NULL
                                    );

                /*
                 * panic() on error.
                 */

                if (error) {
                        
                        panic("approachintersection: thread_fork failed: %s\n",
                              strerror(error)
                              );
                }
        }

	for (i=0; i<NCARS; i++) {
		P(done_sem);
	}

	kprintf("All cars done.\n");
        return 0;
}
