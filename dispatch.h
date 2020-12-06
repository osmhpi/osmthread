
#ifndef _DISPATCHER_H
#define _DISPATCHER_H

#include <ucontext.h>

struct tcontext_t
{
  ucontext_t ucontext;
};


int dispatch_init(struct tcontext_t*, void(*)(void));

int dispatch_context_init(struct tcontext_t*, void(*)(void*), void*);

void dispatch_context_fini(struct tcontext_t*);

void dispatch_swap_context(struct tcontext_t*, struct tcontext_t*);


#endif // _DISPATCHER_H
