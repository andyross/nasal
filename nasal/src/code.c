#include <stdio.h> // DEBUG

#include "nasl.h"
#include "code.h"

// FIXME: need to store a list of all contexts
struct Context globalContext;

char* opStringDEBUG(int op);
void printOpDEBUG(int ip, int op);
void printRefDEBUG(naRef r);
void printStackDEBUG(struct Context* ctx);

static int checkVec(naRef vec, naRef idx)
{
    int i;
    if(!IS_NUM(idx)) ERR("non-numeric vector index"); // FIXME: "1"
    i = (int)idx.num;
    if(i < 0 || i >= vec.ref.ptr.vec->size)
        ERR("vector index out of bounds");
    return i;
}

static naRef containerGet(naRef box, naRef key)
{
    naRef result = naNil();
    if(IS_HASH(box)) {
        if(!naHash_get(box, key, &result))
            ERR("undefined value in container");
    } else if(IS_VEC(box)) {
        result = naVec_get(box, checkVec(box, key));
    } else {
        ERR("extract from non-container");
    }
    return result;
}

static void containerSet(naRef box, naRef key, naRef val)
{
    if     (IS_HASH(box)) naHash_set(box, key, val);
    else if(IS_VEC(box))  naVec_set(box, checkVec(box, key), val);
    else                  ERR("insert into non-container");
}

static void initContext(struct Context* c)
{
    int i;
    for(i=0; i<NUM_NASL_TYPES; i++)
        naGC_init(&(c->pools[i]), naTypeSize(i));

    c->fTop = c->opTop = 0;

    BZERO(c->fStack, MAX_RECURSION * sizeof(struct Frame));
    BZERO(c->opStack, MAX_STACK_DEPTH * sizeof(naRef));

    // Cache pre-calculated "me" and "arg" scalars
    c->meRef = naNewString(c);
    naStr_fromdata(c->meRef, "me", 2);
    c->argRef = naNewString(c);
    naStr_fromdata(c->argRef, "arg", 3);
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
    for(i=0; i < c->fTop; i++) {
        struct Frame* f = &(c->fStack[i]);
        naGC_mark(f->code);
        naGC_mark(f->namespace);
        naGC_mark(f->locals);
    }
    for(i=0; i <= c->opTop-1; i++)
        naGC_mark(c->opStack[i]);

    for(i=0; i<NUM_NASL_TYPES; i++)
        naGC_reap(&(c->pools[i]));

    // FIXME: need to include constants for Parser's during
    // compilation, they aren't referenced anywhere yet!
}

void setupFuncall(struct Context* ctx, naRef closure, naRef args)
{
    struct Frame* f;

    // DEBUG
    if(!IS_CLOSURE(closure)) ERR("DEBUG");
    if(!IS_CODE(closure.ref.ptr.closure->code)) ERR("DEBUG");
    if(!IS_HASH(closure.ref.ptr.closure->namespace)) ERR("DEBUG");
    if(!IS_VEC(args)) ERR("DEBUG");
    // DEBUG

    if(ctx->fTop >= MAX_RECURSION) ERR("recursion too deep");

    f = &(ctx->fStack[ctx->fTop++]);
    f->code = closure.ref.ptr.closure->code;
    f->namespace = closure.ref.ptr.closure->namespace;
    f->locals = naNewHash(ctx);
    f->ip = 0;
    f->bp = ctx->opTop;
    f->line = 0;
    naHash_set(f->locals, ctx->argRef, args);
}

static double numify(naRef o)
{
    double n;
    if(IS_NUM(o)) return o.num;
    else if(!IS_STR(o)) ERR("non-scalar in numeric context");
    else if(naStr_tonum(o, &n)) return n;
    else ERR("non-numeric string in numeric context");
    return 0;
}


static naRef stringify(struct Context* ctx, naRef r)
{
    naRef result;
    if(IS_STR(r)) return r;
    if(IS_NUM(r)) {
        result = naNewString(ctx);
        naStr_fromnum(result, r.num);
        return result;
    }
    ERR("non-scalar in string context");
    return naNil();
}

static naRef evalAndOr(int op, naRef ra, naRef rb)
{
    int a = naTrue(ra);
    int b = naTrue(rb);
    int result;
    if(op == OP_AND) result = a && b ? 1 : 0;
    else             result = a || b ? 1 : 0;
    return naNum(result);
}

static naRef evalEquality(int op, naRef ra, naRef rb)
{
    int result = naEqual(ra, rb);
    return naNum((op==OP_EQ) ? result : !result);
}

static naRef evalBinaryNumeric(int op, naRef ra, naRef rb)
{
    double a = numify(ra), b = numify(rb);
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

// OP_EACH works like a vector get, except that it leaves the vector
// and index on the stack, increments the index after use, and pops
// the arguments and pushes a nil if the index is beyond the end.
static naRef evalEach(struct Context* ctx)
{
    int idx = (int)(ctx->opStack[ctx->opTop-1].num);
    naRef vec = ctx->opStack[ctx->opTop-2];
    if(idx >= vec.ref.ptr.vec->size) {
        ctx->opTop -= 2; // pop two values
        return naNil();
    }
    ctx->opStack[ctx->opTop-1].num = idx+1; // modify in place
    return naVec_get(vec, idx);
}

// When a code object comes out of the constant pool and shows up on
// the stack, it needs to be bound with the lexical context.
// FIXME: transitive binding?
static naRef bindFunction(struct Context* ctx, naRef code, naRef locals)
{
    naRef result = naNewClosure(ctx);
    result.ref.ptr.closure->code = code;
    result.ref.ptr.closure->namespace = locals;
    return result;
}

static naRef getLocal(struct Frame* f, naRef sym)
{
    // Locals first, then the function closure
    naRef result;
    if(!naHash_get(f->locals, sym, &result))
        if(!naHash_get(f->namespace, sym, &result))
            ERR("undefined symbol");
    return result;
}

static naRef setLocal(struct Frame* f, naRef sym, naRef val)
{
    // Put it in locals, unless it isn't defined there already *and*
    // exists in the closure.
    naRef dummy, hash = f->locals;
    if((naHash_get(f->locals,    sym, &dummy) == 0) &&
       (naHash_get(f->namespace, sym, &dummy) == 1))
    {
        hash = f->namespace;
    }
    naHash_set(hash, sym, val);
    return val;
}

static void PUSH(struct Context* ctx, naRef r)
{
    if(ctx->opTop >= MAX_STACK_DEPTH) ERR("stack overflow");
    ctx->opStack[ctx->opTop++] = r;
}

static naRef POP(struct Context* ctx)
{
    if(ctx->opTop == 0) ERR("stack underflow");
    return ctx->opStack[--ctx->opTop];
}

static naRef TOP(struct Context* ctx)
{
    if(ctx->opTop == 0) ERR("stack underflow");
    return ctx->opStack[ctx->opTop-1];
}

static int ARG16(unsigned char* byteCode, struct Frame* f)
{
    int arg = byteCode[f->ip]<<8 | byteCode[f->ip+1];
    f->ip += 2;
    return arg;
}

static void run1(struct Context* ctx)
{
    naRef a, b, c;
    struct Frame* f = &(ctx->fStack[ctx->fTop-1]);
    struct naCode* cd = f->code.ref.ptr.code;
    int op, arg;

    if(f->ip >= cd->nBytes) {
        printf("Done with frame %d\n", ctx->fTop-1);
        ctx->fTop--;
        if(ctx->fTop <= 0)
            ctx->done = 1;
        return;
    }

    op = cd->byteCode[f->ip++];
    printOpDEBUG(f->ip-1, op);
    switch(op) {
    case OP_POP:
        POP(ctx);
        break;
    case OP_DUP:
        PUSH(ctx, ctx->opStack[ctx->opTop-1]);
        break;
    case OP_XCHG:
        a = POP(ctx); b = POP(ctx);
        PUSH(ctx, a); PUSH(ctx, b);
        break;
    case OP_PLUS: case OP_MUL: case OP_DIV: case OP_MINUS:
    case OP_LT: case OP_LTE: case OP_GT: case OP_GTE:
        a = POP(ctx); b = POP(ctx);
        PUSH(ctx, evalBinaryNumeric(op, b, a));
        break;
    case OP_EQ: case OP_NEQ:
        a = POP(ctx); b = POP(ctx);
        PUSH(ctx, evalEquality(op, b, a));
        break;
    case OP_AND: case OP_OR:
        a = POP(ctx); b = POP(ctx);
        PUSH(ctx, evalAndOr(op, a, b));
        break;
    case OP_CAT:
        a = stringify(ctx, POP(ctx)); b = stringify(ctx, POP(ctx));
        c = naNewString(ctx);
        naStr_concat(c, b, a);
        PUSH(ctx, c);
        break;
    case OP_NEG:
        a = POP(ctx);
        PUSH(ctx, naNum(-numify(a)));
        break;
    case OP_NOT:
        a = POP(ctx);
        PUSH(ctx, naNum(naTrue(a) ? 0 : 1));
        break;
    case OP_PUSHCONST:
        a = cd->constants[ARG16(cd->byteCode, f)];
        if(IS_CODE(a)) a = bindFunction(ctx, a, f->locals);
        PUSH(ctx, a);
        break;
    case OP_PUSHONE:
        PUSH(ctx, naNum(1));
        break;
    case OP_PUSHZERO:
        PUSH(ctx, naNum(0));
        break;
    case OP_PUSHNIL:
        PUSH(ctx, naNil());
        break;
    case OP_NEWVEC:
        PUSH(ctx, naNewVector(ctx));
        break;
    case OP_VAPPEND:
        b = POP(ctx); a = TOP(ctx);
        naVec_append(a, b);
        break;
    case OP_NEWHASH:
        PUSH(ctx, naNewHash(ctx));
        break;
    case OP_HAPPEND:
        c = POP(ctx); b = POP(ctx); a = TOP(ctx); // a,b,c: hash, key, val
        naHash_set(a, b, c);
        break;
    case OP_LOCAL:
        a = getLocal(f, POP(ctx));
        PUSH(ctx, a);
        break;
    case OP_SETLOCAL:
        a = POP(ctx); b = POP(ctx);
        PUSH(ctx, setLocal(f, b, a));
        break;
    case OP_MEMBER:
        a = POP(ctx); b = POP(ctx);
        if(!IS_HASH(b)) ERR("non-objects have no members");
        if(naHash_get(b, a, &c) == 0)
            ERR("no such member");
        PUSH(ctx, c);
        break;
    case OP_SETMEMBER:
        c = POP(ctx); b = POP(ctx); a = POP(ctx); // a,b,c: hash, key, val
        if(!IS_HASH(a)) ERR("non-objects have no members");
        naHash_set(a, b, c);
        PUSH(ctx, c);
        break;
    case OP_INSERT:
        c = POP(ctx); b = POP(ctx); a = POP(ctx); // a,b,c: box, key, val
        containerSet(a, b, c);
        PUSH(ctx, c);
        break;
    case OP_EXTRACT:
        b = POP(ctx); a = POP(ctx); // a,b: box, key
        PUSH(ctx, containerGet(a, b));
        break;
    case OP_JMP:
        f->ip = ARG16(cd->byteCode, f);
        break;
    case OP_JIFNIL:
        arg = ARG16(cd->byteCode, f);
        a = POP(ctx);
        if(IS_NIL(a))
            f->ip = arg;
        break;
    case OP_JIFNOT:
        arg = ARG16(cd->byteCode, f);
        if(!naTrue(POP(ctx)))
            f->ip = arg;
        break;
    case OP_FCALL:
        b = POP(ctx); a = POP(ctx); // a,b = func, args
        setupFuncall(ctx, a, b);
        f = &(ctx->fStack[ctx->fTop-1]); // fixup local variable
        break;
    case OP_MCALL:
        c = POP(ctx); b = POP(ctx); a = POP(ctx); // a,b,c = obj, func, args
        setupFuncall(ctx, b, c);
        f = &(ctx->fStack[ctx->fTop-1]); // as above
        naHash_set(f->locals, ctx->meRef, a);
        break;
    case OP_RETURN:
        a = POP(ctx);
        ctx->opTop = f->bp; // restore the correct stack frame!
        ctx->fTop--;
        f = &(ctx->fStack[ctx->fTop-1]); // ditto
        PUSH(ctx, a);
        break;
    case OP_LINE:
        f->line = ARG16(cd->byteCode, f);
        break;
    case OP_EACH:
        PUSH(ctx, evalEach(ctx));
        break;
    default:
        ERR("bad opcode");
    }

    if(ctx->fTop <= 0)
        ctx->done = 1;
}

void naRun(struct Context* ctx, naRef code)
{
    naRef closure;
    closure = bindFunction(ctx, code, naNewHash(ctx));
    setupFuncall(ctx, closure, naNewVector(ctx));

    ctx->done = 0;
    while(!ctx->done) {
        run1(ctx);
        printStackDEBUG(ctx); // DEBUG
    }
    printf("DONE\n");
}
