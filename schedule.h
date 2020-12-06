
#ifndef _SCHEDULER_H
#define _SCHEDULER_H

struct osmthread_t
{
  int tid;
};
typedef struct osmthread_t osmthread_t;


void osmthread_yield();
void osmthread_exit();

int osmthread_create(osmthread_t*, void(*)(void*), void*, int);
int osmthread_join(osmthread_t);

#endif // _SCHEDULER_H
