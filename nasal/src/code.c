#include "nasl.h"
#include "code.h"

// FIXME: need to store a list of all contexts
struct Context globalContext;

static void initContext(struct Context* c)
{
    int i;

    // Start off with space for 8 calls.
    c->top = -1;
    c->depth = 8;
    c->stack = ALLOC(c->depth * sizeof(struct Frame));

    for(i=0; i<NUM_NASL_TYPES; i++)
        naGC_init(&(c->pools[i]), naTypeSize(i));
}

struct Context* naNewContext()
{
    // FIXME: make need more than one!
    struct Context* c = &globalContext;
    if(c->stack != 0) *(int*)0=0;
    initContext(c);
    return c;
}

void naGarbageCollect()
{
    int i, j;
    struct Context* c = &globalContext;
    for(i=0; i <= c->top; i++) {
        struct Frame* f = &(c->stack[i]);
        naGC_mark(f->func);
        naGC_mark(f->namespace);
        for(j=0; j <= f->opTop; j++)
            naGC_mark(f->opStack[j]);
    }
    for(i=0; i<NUM_NASL_TYPES; i++)
        naGC_reap(&(c->pools[i]));

    // FIXME: need to include constants for Parser's during compilation
}


