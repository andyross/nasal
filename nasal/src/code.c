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

static inline void PUSH(struct Context* ctx, naRef r)
{
    ctx->opStack[ctx->opTop++] = r;
}

static inline naRef POP(struct Context* ctx)
{
    return ctx->opStack[--ctx->opTop];
}

static inline int ARG16(unsigned char* byteCode, struct Frame* f)
{
    int arg = byteCode[f->ip]<<8 | byteCode[f->ip+1];
    f->ip += 2;
    return arg;
}

void run1(struct Context* ctx)
{
    naRef a, b;
    struct Frame* f = &(ctx->fStack[ctx->fTop-1]);
    struct naCode* cd = f->code.ref.ptr.code;
    int i, op = cd->byteCode[f->ip++];

    switch(op) {
    case OP_PLUS: case OP_MINUS: case OP_MUL: case OP_DIV:
    case OP_LT: case OP_LTE: case OP_GT: case OP_GTE:
        printf("   binop %d\n", op);
        a = POP(ctx); b = POP(ctx);
        PUSH(ctx, binaryNumeric(op, a, b));
        break;
    case OP_PUSHCONST:
        i = ARG16(cd->byteCode, f);
        printf("   pushconst %d\n", i);
        PUSH(ctx, cd->constants[i]);
        break;
    }

    // Are we done now?
    if(f->ip >= cd->nBytes)
        ctx->done = 1;
}

void printRefDEBUG(naRef r);
void printStack(struct Context* ctx)
{
    int i;
    printf("Stack:\n");
    for(i=ctx->opTop-1; i>=0; i--)
        printRefDEBUG(ctx->opStack[i]);
    printf("--\n");
}


void naRun(struct Context* ctx, naRef code)
{
    naRef namespace, closure;

    namespace = naNewHash(ctx);
    
    closure = naNewClosure(ctx);
    closure.ref.ptr.closure->code = code;
    closure.ref.ptr.closure->namespace = namespace;

    { // DEBUG
        int i;
        struct naCode* c = code.ref.ptr.code;
        printf("Constants:\n");
        for(i=0; i<c->nConstants; i++) {
            printf("%d ", i);
            printRefDEBUG(c->constants[i]);
        }
        printf("--\n");
    } // DEBUG

    setupFuncall(ctx, closure);

    ctx->done = 0;
    while(!ctx->done) {
        printStack(ctx); // DEBUG
        run1(ctx);
    }

    printf("DONE:\n"); // DEBUG 
    printStack(ctx); // DEBUG
}
