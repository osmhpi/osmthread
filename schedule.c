
#include "schedule.h"
#include "dispatch.h"

#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#define MAX_THREADS 64
#define MAX_PRIORITY 32
#define QUANTUM_OF_SCHEDULING (20 * 1000 * 1000) // 20ms


// enumeration of possible thread states
// (there is no waiting state, threads can only run or be ready)
enum thread_state
{
  unset      = 0,
  running    = 1,
  ready      = 2,
  terminated = 3
};

// a thread control block -- this holds all metadata for scheduling
// ******
// *TODO: extend this struct and rr_* to avoid thread starvation
// ******
struct tblock_t
{
  struct tcontext_t context;
  enum thread_state state;
  int base_priority;
};

static struct tblock_t threads[MAX_THREADS];
static struct tblock_t *current_thread;

// the scheduling queues, implemented as list of ring buffers
struct rrqueue_t
{
  struct tblock_t *queue[MAX_THREADS];
  int left;
  int right;
};

static struct rrqueue_t queues[MAX_PRIORITY];

// a posix timer for rudimentary preemption
static timer_t timer;
static struct itimerspec timer_armed;
static struct itimerspec timer_disarmed;


static int
get_next_tid (void)
{
  // iterate over the list of threads and find a free slot
  unsigned int i;
  for (i = 0; i < MAX_THREADS; ++i)
    if (threads[i].state == unset)
      return i;

  // if none is found, return -1 to indicate en error
  return -1;
}

void
rr_enqueue (struct tblock_t *tb)
{
  // ******
  // *TODO: extend this function, and/or rr_dequeue to avoid starvation
  // ******

  // find the matching queue
  struct rrqueue_t *queue = &queues[tb->base_priority];

  // and insert to the right
  queue->queue[queue->right++] = tb;
  queue->right %= MAX_THREADS;
}

struct tblock_t*
rr_dequeue (void)
{
  // ******
  // *TODO: extend this function, and/or rr_enqueue to avoid starvation
  // ******

  // find the nonempty queue with highest priority (0 is highest)
  unsigned int i;
  for (i = 0; i < MAX_PRIORITY; ++i)
    if (queues[i].left != queues[i].right)
      break;
  struct rrqueue_t *queue = &queues[i];

  // and remove and return the left element
  struct tblock_t *res = queue->queue[queue->left++];
  queue->left %= MAX_THREADS;

  return res;
}

void
osmthread_yield (void)
{
  fprintf(stderr, "thread #%li yield\n", current_thread - threads);
  timer_settime(timer, 0, &timer_disarmed, 0);

  // mark the current thread as ready and enqueue
  current_thread->state = ready;
  rr_enqueue(current_thread);
  struct tblock_t *last = current_thread;

  // dequeue next thread and set as current
  current_thread = rr_dequeue();
  current_thread->state = running;

  // dispatch
  timer_settime(timer, 0, &timer_armed, 0);
  dispatch_swap_context(&current_thread->context, &last->context);
}

void
osmthread_exit (void)
{
  fprintf(stderr, "thread #%li exit\n", current_thread - threads);
  timer_settime(timer, 0, &timer_disarmed, 0);

  // mark current thread terminated
  current_thread->state = terminated;

  // dequeue next thread and set as current
  current_thread = rr_dequeue();
  current_thread->state = running;

  // dispatch
  timer_settime(timer, 0, &timer_armed, 0);
  dispatch_swap_context(&current_thread->context, NULL);
}

static void
osmthread_preempt (int arg)
{
  (void)arg; // dummy argument usage to silence warnings

  fprintf(stderr, "thread #%li preempt\n", current_thread - threads);
  timer_settime(timer, 0, &timer_disarmed, 0);

  // mark the current thread as ready and enqueue
  current_thread->state = ready;
  rr_enqueue(current_thread);
  struct tblock_t *last = current_thread;

  // dequeue next thread and set as current
  current_thread = rr_dequeue();
  current_thread->state = running;

  // dispatch
  timer_settime(timer, 0, &timer_armed, 0);
  dispatch_swap_context(&current_thread->context, &last->context);
}

static int
osmthread_sched_init (void)
{
  // setup the main thread struct
  threads[0].state = running;
  threads[0].base_priority = MAX_PRIORITY / 2;
  current_thread = &threads[0];

  // init the dispatcher, and register the main thread and exit handler
  int res = dispatch_init(&threads[0].context, &osmthread_exit);
  if (res)
    return res;

  // setup a signal handler and timer for preemption
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  sa.sa_handler = &osmthread_preempt;

  res = sigaction(SIGALRM, &sa, NULL);
  if (res != 0)
    return res;

  timer_armed.it_value.tv_nsec = QUANTUM_OF_SCHEDULING;
  timer_create(CLOCK_THREAD_CPUTIME_ID, NULL, &timer);
  timer_settime(timer, 0, &timer_armed, 0);

  return 0;
}

int
osmthread_create (osmthread_t *thread, void(*start_routine)(void*), void *arg, int nice)
{
  // threads[0] is the main thread.
  // if this is unset, we need to initialize first
  if (threads[0].state == unset)
    {
      int res = osmthread_sched_init();
      if (res != 0)
        return res;
    }

  // try to produce a free thread id
  thread->tid = get_next_tid();
  if (thread->tid < 0)
    {
      errno = EAGAIN;
      return errno;
    }

  // prepare the thread block and the context
  struct tblock_t *tb = threads + thread->tid;
  int res = dispatch_context_init(&tb->context, start_routine, arg);
  if (res != 0)
    return res;

  // enqueue the new thread
  tb->state = ready;
  tb->base_priority = MAX_PRIORITY / 2 + nice;
  rr_enqueue(tb);

  return 0;
}

int
osmthread_join (osmthread_t thread)
{
  // get thread block by tid
  struct tblock_t *tb = threads + thread.tid;
  if (tb->state == unset)
    {
      errno = EINVAL;
      return errno;
    }

  // wait for thread completion -- by yielding to other threads
  while (tb->state != terminated)
    osmthread_yield();

  // thread terminated -- release thread resources and return
  tb->state = unset;
  dispatch_context_fini(&tb->context);

  return 0;
}
