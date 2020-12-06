
#include "dispatch.h"

#include <stdlib.h>

#define THREAD_STACK 32 * 1024 // 32KiB


// a separate context as a fall-through handler for exiting threads
static ucontext_t exit_context;


int
dispatch_init (struct tcontext_t *main_context, void(*exit_handler)(void))
{
  // produce a context for the main thread
  int res = getcontext(&main_context->ucontext);
  if (res != 0)
    return res;

  // produce a context for the exit handler
  res = getcontext(&exit_context);
  if (res)
    return res;

  // allocate a stack
  exit_context.uc_stack.ss_size = THREAD_STACK;
  exit_context.uc_stack.ss_sp = malloc(THREAD_STACK);
  if (!exit_context.uc_stack.ss_sp)
    return -1;

  // create the context from the given handler function
  makecontext(&exit_context, exit_handler, 0);

  return 0;
}

int
dispatch_context_init (struct tcontext_t *tc, void(*start_routine)(void*), void *arg)
{
  // produce a context for the given thread
  int res = getcontext(&tc->ucontext);
  if (res)
    return res;

  // allocate a stack
  tc->ucontext.uc_stack.ss_size = THREAD_STACK;
  tc->ucontext.uc_stack.ss_sp = malloc(THREAD_STACK);
  if (!tc->ucontext.uc_stack.ss_sp)
    return -1;

  // register the exit handler as followup context
  tc->ucontext.uc_link = &exit_context;

  // create the context fom the given start function
  makecontext(&tc->ucontext, (void(*)(void))start_routine, 1, arg);

  return 0;
}

void
dispatch_context_fini (struct tcontext_t *tc)
{
  // free the stack
  free(tc->ucontext.uc_stack.ss_sp);
  tc->ucontext.uc_stack.ss_sp = NULL;
  tc->ucontext.uc_stack.ss_size = 0;
}

void
dispatch_swap_context (struct tcontext_t *next, struct tcontext_t *prev)
{
  // if no previous context was given (thread terminated) use setcontext
  // (this discards the current context)
  if (!prev)
    setcontext(&next->ucontext);

  // otherwise, use swapcontext, which retains the current context
  // (this allows us to swap back to the current context, and resume execution here)
  swapcontext(&prev->ucontext, &next->ucontext);
}


