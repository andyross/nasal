#include <stdio.h> // DEBUG

#include "nasl.h"
#include "code.h"

// FIXME: need to store a list of all contexts
struct Context globalContext;

static void initContext(struct Context* c)
{
    int i;
    for(i=0; i<NUM_NASL_TYPES; i++)
        naGC_init(&(c->pools[i]), naTypeSize(i));

    c->fTop = c->opTop = 0;

    BZERO(c->fStack, MAX_RECURSION * sizeof(struct Frame));
    BZERO(c->opStack, MAX_STACK_SIZE * sizeof(naRef));
}

struct Context* naNewContext()
{
    // FIXME: need more than one!
    struct Context* c = &globalContext;
    initContext(c);
    return c;
}

void naGarbageCollect()
{
    int i;
    struct Context* c = &globalContext; // FIXME: more than one!
    for(i=0; i <= c->fTop; i++) {
        struct Frame* f = &(c->fStack[i]);
        naGC_mark(f->code);
        naGC_mark(f->namespace);
        naGC_mark(f->locals);
    }
    for(i=0; i <= c->opTop; i++)
        naGC_mark(c->opStack[i]);

    for(i=0; i<NUM_NASL_TYPES; i++)
        naGC_reap(&(c->pools[i]));

    // FIXME: need to include constants for Parser's during
    // compilation, they aren't referenced anywhere yet!
}

void setupFuncall(struct Context* ctx, naRef closure)
{
    struct Frame* f;

    if(ctx->fTop >= MAX_RECURSION) ERR("recursion too deep");

    f = &(ctx->fStack[ctx->fTop++]);
    f->code = closure.ref.ptr.closure->code;
    f->namespace = closure.ref.ptr.closure->namespace;
    f->locals = naNewHash(ctx);
    f->ip = 0;
    f->bp = ctx->opTop;
}

static double numify(naRef o)
{
    if(IS_NUM(o)) return o.num;
    else if(!IS_STR(o)) ERR("non-scalar in numeric context");
    return (naStr_tonum(o)).num;
    
}

static naRef binaryNumeric(int op, naRef oa, naRef ob)
{
    double a = numify(oa), b = numify(ob);
    switch(op) {
    case OP_PLUS:  return naNum(a + b);
    case OP_MINUS: return naNum(a - b);
    case OP_MUL:   return naNum(a * b);
    case OP_DIV:   return naNum(a / b);
    case OP_LT:    return naNum(a < b ? 1 : 0);
    case OP_LTE:   return naNum(a <= b ? 1 : 0);
    case OP_GT:    return naNum(a > b ? 1 : 0);
    case OP_GTE:   return naNum(a >= b ? 1 : 0);
    }
    return naNil();
}

#define ARG0 ctx->opStack[ctx->opTop-1]
#define ARG1 ctx->opStack[ctx->opTop-2]
#define REPLACE(n,v) ctx->opTop-=((n)-1);ctx->opStack[ctx->opTop]=(v)
void run1(struct Context* ctx)
{
    naRef val;
    struct Frame* f = &(ctx->fStack[ctx->fTop-1]);
    struct naCode* c = f->code.ref.ptr.code;
    int op = c->byteCode[f->ip++];

    switch(op) {
    case OP_PLUS: case OP_MINUS: case OP_MUL: case OP_DIV:
    case OP_LT: case OP_LTE: case OP_GT: case OP_GTE:
        val = binaryNumeric(op, ARG0, ARG1);
        REPLACE(2, val);
    }

    // Are we done now?
    if(f->ip >= c->nBytes)
        ctx->done = 1;
}

void naRun(struct Context* ctx, naRef code)
{
    naRef namespace, closure;

    namespace = naNewHash(ctx);

    closure = naNewClosure(ctx);
    closure.ref.ptr.closure->code = code;
    closure.ref.ptr.closure->namespace = namespace;

    setupFuncall(ctx, closure);

    ctx->done = 0;
    while(!ctx->done)
        run1(ctx);
}
