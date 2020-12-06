
#include "schedule.h"

#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_THREADS 10
#define TIMEOUT 2.0 // seconds
#define SPIN_CYCLES 100000


static time_t thread_times[NUM_THREADS];


static void*
threads_check_starvation (void *arg)
{
  /* this is the thread function of the starvation watchdog
   *
   * it checks periodically whether one of the worker threads has missed its
   * heartbeat update to the thread_times array.
   */

  (void)arg; // arg is usused. cast to void to make it explicit.

  while (1)
    {
      time_t now = time(NULL);

      // check all threads, and see if one timed out
      unsigned int i;
      for (i = 0; i < NUM_THREADS; ++i)
        {
          if (difftime(now, thread_times[i]) > TIMEOUT)
            {
              fprintf(stderr, "thread #%u starved\n", i + 1);
              exit(1);
            }
        }
    }
}

static void
thread_func (void *arg)
{

  /* this is the thread function of the worker threads
   *
   * it periodically writes a heartbeat to the thread_times array and does
   * pretty much no other useful work.
   */

  unsigned int thread_id = *(unsigned int*)arg;

  // periodically update the threads last active time
  while (1)
    {
      thread_times[thread_id] = time(NULL);

      // pretend to do some work
      volatile int i;
      for (i = 0; i < SPIN_CYCLES; ++i);
    }
}

int
main (void)
{
  // start the threads
  static osmthread_t threads[NUM_THREADS];
  int args[NUM_THREADS];

  unsigned int i;
  for (i = 0; i < NUM_THREADS; ++i)
    {
      args[i] = i;
      thread_times[i] = time(NULL);

      // give the last thread should a lower priority, to force it to starve
      int nice = i == NUM_THREADS - 1 ? 1 : 0;

      if (osmthread_create(threads + i, &thread_func, args + i, nice))
        {
          perror("osmthread_create");
          return 1;
        }
    }

  // start the starvation watchdog
  pthread_t pt;
  if (pthread_create(&pt, NULL, &threads_check_starvation, NULL))
    {
      perror("pthread_create");
      return 1;
    }

  // wait for starvation
  for (i = 0; i < NUM_THREADS; ++i)
    osmthread_join(threads[i]);
  pthread_join(pt, NULL);

  return 0;
}
