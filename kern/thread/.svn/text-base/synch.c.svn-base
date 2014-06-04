/*
 * Synchronization primitives.
 * See synch.h for specifications of the functions.
 */

#include <types.h>
#include <lib.h>
#include <synch.h>
#include <thread.h>
#include <curthread.h>
#include <machine/spl.h>
#include <queue.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *namearg, int initial_count)
{
	struct semaphore *sem;

	assert(initial_count >= 0);

	sem = kmalloc(sizeof(struct semaphore));
	if (sem == NULL) {
		return NULL;
	}

	sem->name = kstrdup(namearg);
	if (sem->name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->count = initial_count;
	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	spl = splhigh();
	assert(thread_hassleepers(sem)==0);
	splx(spl);

	/*
	 * Note: while someone could theoretically start sleeping on
	 * the semaphore after the above test but before we free it,
	 * if they're going to do that, they can just as easily wait
	 * a bit and start sleeping on the semaphore after it's been
	 * freed. Consequently, there's not a whole lot of point in 
	 * including the kfrees in the splhigh block, so we don't.
	 */

	kfree(sem->name);
	kfree(sem);
}

void 
P(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	assert(in_interrupt==0);

	spl = splhigh();
	while (sem->count==0) {
		thread_sleep(sem);
	}
	assert(sem->count>0);
	sem->count--;
	splx(spl);
}

void
V(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);
	spl = splhigh();
	sem->count++;
	assert(sem->count>0);
	thread_wakeup(sem);
	splx(spl);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(struct lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->name = kstrdup(name);
	if (lock->name == NULL) {
		kfree(lock);
		return NULL;
	}
	
	lock->held = 0;

	lock->current_holder = NULL;
	
	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);
	assert(lock->held == 0);
	assert(lock->current_holder == NULL);	
	kfree(lock->name);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
	int spl;
	spl = splhigh();
	while (lock->held) {
		thread_sleep(lock);
	}
	assert(lock->held == 0);
	lock->held = 1;
	assert(lock->current_holder == NULL);
	lock->current_holder = curthread;
	splx(spl);
}

void
lock_release(struct lock *lock)
{
	int spl;	

	if(lock_do_i_hold(lock)){
		spl = splhigh();
		lock->held = 0;
		lock->current_holder = NULL;
		thread_wakeup(lock);
		splx(spl);
	}
}

int
lock_do_i_hold(struct lock *lock)
{
	return (lock->current_holder == curthread) ? 1 : 0;
}

////////////////////////////////////////////////////////////
//
// CV

struct cv {
	char *name;
	struct queue *cv_threads_q;
};

struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(struct cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->name = kstrdup(name);
	if (cv->name==NULL) {
		kfree(cv);
		return NULL;
	}
	//16 seemed like a reasonable number of threads waiting on a single CV
	cv->cv_threads_q = q_create(16);
		if (cv->cv_threads_q == NULL) {
			panic("scheduler: Could not create run cv_threads_q\n");
		}
	
	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);
	//fails if there are still threads waiting on this cv
	q_destroy(cv->cv_threads_q);
	
	kfree(cv->name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	int spl;
	int result;
	void *addr;

	assert(cv != NULL);
	assert(lock != NULL);

	spl = splhigh();

	assert(lock_do_i_hold(lock));
	addr = curthread;
	//Add the current thread to the queue
	result = q_addtail(cv->cv_threads_q, addr);
	if(result) return;
	//Release lock
	lock_release(lock);
	//Go to sleep
	thread_sleep(addr);
	//Re-acquire lock
	lock_acquire(lock);

	splx(spl);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{

	int spl;
	void *addr;
	assert(cv != NULL);
	assert(lock != NULL);
	
	if(!q_empty(cv->cv_threads_q)){
		spl = splhigh();
		addr = q_remhead(cv->cv_threads_q);
		thread_wakeup(addr);
		splx(spl);
	}
	//else kprintf("signal lost\n");//test
	// else no one waiting for signal - signal is lost (do nothing)
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	int spl;
	void *addr;
	assert(cv != NULL);
	assert(lock != NULL);
	spl = splhigh();
	while(!q_empty(cv->cv_threads_q)){
		addr = q_remhead(cv->cv_threads_q);
		thread_wakeup(addr);
	}
	splx(spl);
}
