/*****************************************************************************
 * Copyright 2019 Operating Systems & Middleware Group, HPI
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *****************************************************************************/

#include "schedule.h"

#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_THREADS 10
#define TIMEOUT 2.0


static time_t thread_times[NUM_THREADS];


static void*
threads_check_starvation (void *arg)
{
  (void)arg; // dummy argument usage to silence warnings

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
  unsigned int thread_id = *(unsigned int*)arg;

  // periodically update the threads last active time
  while (1)
    {
      thread_times[thread_id] = time(NULL);
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

      // the last thread should have a lower priority
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
