#include "nasl.h"
#include "code.h"

// FIXME: need to store a list of all contexts
struct Context globalContext;

// DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG 
// Uncomment the DBG macro to print debug stuff (operator/address,
// stack contents, etc...)
#define DBG(expr) /* expr */
#include <stdio.h> // DEBUG
#include <stdlib.h> // DEBUG
char* opStringDEBUG(int op);
void printOpDEBUG(int ip, int op);
void printRefDEBUG(naRef r);
void printStackDEBUG(struct Context* ctx);
// DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG 

#define ERR(c, msg) naRuntimeError((c),(msg))
void naRuntimeError(struct Context* c, char* msg)
{ 
    c->error = msg;
    longjmp(c->jumpHandle, 1);
}

int boolify(struct Context* ctx, naRef r)
{
    if(IS_NIL(r)) return 0;
    if(IS_NUM(r)) return r.num != 0;
    if(IS_STR(r)) return 1;
    ERR(ctx, "non-scalar used in boolean context");
    return 0;
}

static double numify(struct Context* ctx, naRef o)
{
    double n;
    if(IS_NUM(o)) return o.num;
    else if(IS_NIL(o)) ERR(ctx, "nil used in numeric context");
    else if(!IS_STR(o)) ERR(ctx, "non-scalar in numeric context");
    else if(naStr_tonum(o, &n)) return n;
    else ERR(ctx, "non-numeric string in numeric context");
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
    ERR(ctx, "non-scalar in string context");
    return naNil();
}

static int checkVec(struct Context* ctx, naRef vec, naRef idx)
{
    int i = (int)numify(ctx, idx);
    if(i < 0 || i >= vec.ref.ptr.vec->size)
        ERR(ctx, "vector index out of bounds");
    return i;
}

static naRef containerGet(struct Context* ctx, naRef box, naRef key)
{
    naRef result = naNil();
    if(!IS_SCALAR(key)) ERR(ctx, "container index not scalar");
    if(IS_HASH(box)) {
        if(!naHash_get(box, key, &result))
            ERR(ctx, "undefined value in container");
    } else if(IS_VEC(box)) {
        result = naVec_get(box, checkVec(ctx, box, key));
    } else {
        ERR(ctx, "extract from non-container");
    }
    return result;
}

static void containerSet(struct Context* ctx, naRef box, naRef key, naRef val)
{
    if(!IS_SCALAR(key))   ERR(ctx, "container index not scalar");
    else if(IS_HASH(box)) naHash_set(box, key, val);
    else if(IS_VEC(box))  naVec_set(box, checkVec(ctx, box, key), val);
    else                  ERR(ctx, "insert into non-container");
}

static void initContext(struct Context* c)
{
    int i;
    for(i=0; i<NUM_NASL_TYPES; i++)
        naGC_init(&(c->pools[i]), naTypeSize(i));

    c->fTop = c->opTop = c->markTop = 0;

    naBZero(c->fStack, MAX_RECURSION * sizeof(struct Frame)); // DEBUG
    naBZero(c->opStack, MAX_STACK_DEPTH * sizeof(naRef)); // DEBUG

    c->parserTemporaries = naNewVector(c);

    // Cache pre-calculated "me", "arg" and "parents" scalars
    c->meRef = naStr_fromdata(naNewString(c), "me", 2);
    c->argRef = naStr_fromdata(naNewString(c), "arg", 3);
    c->parentsRef = naStr_fromdata(naNewString(c), "parents", 7);
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
        naGC_mark(f->func);
        naGC_mark(f->locals);
    }
    for(i=0; i <= c->opTop-1; i++)
        naGC_mark(c->opStack[i]);

    naGC_mark(c->parserTemporaries);

    for(i=0; i<NUM_NASL_TYPES; i++)
        naGC_reap(&(c->pools[i]));
}

void setupFuncall(struct Context* ctx, naRef func, naRef args)
{
    struct Frame* f;
    if(ctx->fTop >= MAX_RECURSION) ERR(ctx, "recursion too deep");
    f = &(ctx->fStack[ctx->fTop++]);
    f->func = func;
    f->ip = 0;
    f->bp = ctx->opTop;
    f->line = 0;
    f->locals = naNewHash(ctx);
    f->args = args;
    naHash_set(f->locals, ctx->argRef, args);
}

static naRef evalAndOr(struct Context* ctx, int op, naRef ra, naRef rb)
{
    int a = boolify(ctx, ra);
    int b = boolify(ctx, rb);
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

static naRef evalBinaryNumeric(struct Context* ctx, int op, naRef ra, naRef rb)
{
    double a = numify(ctx, ra), b = numify(ctx, rb);
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

// When a code object comes out of the constant pool and shows up on
// the stack, it needs to be bound with the lexical context.
static naRef bindFunction(struct Context* ctx, struct Frame* f, naRef code)
{
    naRef next = f->func.ref.ptr.func->closure;
    naRef closure = naNewClosure(ctx, f->locals, next);
    naRef result = naNewFunc(ctx, code, closure);
    return result;
}

static int getClosure(naRef closure, naRef sym, naRef* result)
{
    struct naClosure* c = closure.ref.ptr.closure;
    if(c == 0) return 0;
    else if(naHash_get(c->namespace, sym, result)) return 1;
    else return getClosure(c->next, sym, result);
}

// Get a local symbol, or check the closure list if it isn't there
static naRef getLocal(struct Context* ctx, struct Frame* f, naRef sym)
{
    naRef result;
    if(!naHash_get(f->locals, sym, &result)) {
        if(!getClosure(f->func.ref.ptr.func->closure, sym, &result))
            ERR(ctx, "undefined symbol");
    }
    return result;
}

static int setClosure(naRef closure, naRef sym, naRef val)
{
    struct naClosure* c = closure.ref.ptr.closure;
    if(c == 0) { return 0; }
    else if(naHash_tryset(c->namespace, sym, val)) { return 1; }
    else { return setClosure(c->next, sym, val); }
}

static naRef setLocal(struct Frame* f, naRef sym, naRef val)
{
    // Try the locals first, if not already there try the closures in
    // order.  Finally put it in the locals if nothing matched.
    if(!naHash_tryset(f->locals, sym, val))
        if(!setClosure(f->func.ref.ptr.func->closure, sym, val))
            naHash_set(f->locals, sym, val);
    return val;
}

static void PUSH(struct Context* ctx, naRef r)
{
    if(ctx->opTop >= MAX_STACK_DEPTH) ERR(ctx, "stack overflow");
    ctx->opStack[ctx->opTop++] = r;
}

static naRef POP(struct Context* ctx)
{
    if(ctx->opTop == 0) ERR(ctx, "BUG: stack underflow");
    return ctx->opStack[--ctx->opTop];
}

static naRef TOP(struct Context* ctx)
{
    if(ctx->opTop == 0) ERR(ctx, "BUG: stack underflow");
    return ctx->opStack[ctx->opTop-1];
}

static int ARG16(unsigned char* byteCode, struct Frame* f)
{
    int arg = byteCode[f->ip]<<8 | byteCode[f->ip+1];
    f->ip += 2;
    return arg;
}

// OP_EACH works like a vector get, except that it leaves the vector
// and index on the stack, increments the index after use, and pops
// the arguments and pushes a nil if the index is beyond the end.
static void evalEach(struct Context* ctx)
{
    int idx = (int)(ctx->opStack[ctx->opTop-1].num);
    naRef vec = ctx->opStack[ctx->opTop-2];
    if(idx >= vec.ref.ptr.vec->size) {
        ctx->opTop -= 2; // pop two values
        PUSH(ctx, naNil());
        return;
    }
    ctx->opStack[ctx->opTop-1].num = idx+1; // modify in place
    PUSH(ctx, naVec_get(vec, idx));
}

// Recursively descend into the parents lists
static int getMember(struct Context* ctx, naRef obj, naRef fld, naRef* result)
{
    naRef p;
    if(!IS_HASH(obj)) ERR(ctx, "non-objects have no members");
    if(naHash_get(obj, fld, result)) {
        return 1;
    } else if(naHash_get(obj, ctx->parentsRef, &p)) {
        int i;
        if(!IS_VEC(p)) ERR(ctx, "parents field not vector");
        for(i=0; i<p.ref.ptr.vec->size; i++)
            if(getMember(ctx, p.ref.ptr.vec->array[i], fld, result))
                return 1;
    }
    return 0;
}

static void run1(struct Context* ctx, struct Frame* f, naRef code)
{
    naRef a, b, c;
    struct naCode* cd = code.ref.ptr.code;
    int op, arg;

    if(f->ip >= cd->nBytes) {
        DBG(printf("Done with frame %d\n", ctx->fTop-1);)
        ctx->fTop--;
        if(ctx->fTop <= 0)
            ctx->done = 1;
        return;
    }

    op = cd->byteCode[f->ip++];
    DBG(printOpDEBUG(f->ip-1, op);)
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
        PUSH(ctx, evalBinaryNumeric(ctx, op, b, a));
        break;
    case OP_EQ: case OP_NEQ:
        a = POP(ctx); b = POP(ctx);
        PUSH(ctx, evalEquality(op, b, a));
        break;
    case OP_AND: case OP_OR:
        a = POP(ctx); b = POP(ctx);
        PUSH(ctx, evalAndOr(ctx, op, a, b));
        break;
    case OP_CAT:
        a = stringify(ctx, POP(ctx)); b = stringify(ctx, POP(ctx));
        c = naNewString(ctx);
        naStr_concat(c, b, a);
        PUSH(ctx, c);
        break;
    case OP_NEG:
        a = POP(ctx);
        PUSH(ctx, naNum(-numify(ctx, a)));
        break;
    case OP_NOT:
        a = POP(ctx);
        PUSH(ctx, naNum(boolify(ctx, a) ? 0 : 1));
        break;
    case OP_PUSHCONST:
        a = cd->constants[ARG16(cd->byteCode, f)];
        if(IS_CODE(a)) a = bindFunction(ctx, f, a);
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
        a = getLocal(ctx, f, POP(ctx));
        PUSH(ctx, a);
        break;
    case OP_SETLOCAL:
        a = POP(ctx); b = POP(ctx);
        PUSH(ctx, setLocal(f, b, a));
        break;
    case OP_MEMBER:
        a = POP(ctx); b = POP(ctx);
        if(!getMember(ctx, b, a, &c))
            ERR(ctx, "no such member");
        PUSH(ctx, c);
        break;
    case OP_SETMEMBER:
        c = POP(ctx); b = POP(ctx); a = POP(ctx); // a,b,c: hash, key, val
        if(!IS_HASH(a)) ERR(ctx, "non-objects have no members");
        naHash_set(a, b, c);
        PUSH(ctx, c);
        break;
    case OP_INSERT:
        c = POP(ctx); b = POP(ctx); a = POP(ctx); // a,b,c: box, key, val
        containerSet(ctx, a, b, c);
        PUSH(ctx, c);
        break;
    case OP_EXTRACT:
        b = POP(ctx); a = POP(ctx); // a,b: box, key
        PUSH(ctx, containerGet(ctx, a, b));
        break;
    case OP_JMP:
        f->ip = ARG16(cd->byteCode, f);
        DBG(printf("   [Jump to: %d]\n", f->ip);)
        break;
    case OP_JIFNIL:
        arg = ARG16(cd->byteCode, f);
        a = TOP(ctx);
        if(IS_NIL(a)) {
            POP(ctx); // Pops **ONLY** if it's nil!
            f->ip = arg;
            DBG(printf("   [Jump to: %d]\n", f->ip);)
        }
        break;
    case OP_JIFNOT:
        arg = ARG16(cd->byteCode, f);
        if(!boolify(ctx, POP(ctx))) {
            f->ip = arg;
            DBG(printf("   [Jump to: %d]\n", f->ip);)
        }
        break;
    case OP_FCALL:
        b = POP(ctx); a = POP(ctx); // a,b = func, args
        setupFuncall(ctx, a, b);
        break;
    case OP_MCALL:
        c = POP(ctx); b = POP(ctx); a = POP(ctx); // a,b,c = obj, func, args
        setupFuncall(ctx, b, c);
        naHash_set(ctx->fStack[ctx->fTop-1].locals, ctx->meRef, a);
        break;
    case OP_RETURN:
        a = POP(ctx);
        ctx->opTop = f->bp; // restore the correct stack frame!
        ctx->fTop--;
        PUSH(ctx, a);
        break;
    case OP_LINE:
        f->line = ARG16(cd->byteCode, f);
        break;
    case OP_EACH:
        evalEach(ctx);
        break;
    case OP_MARK: // save stack state (e.g. "setjmp")
        ctx->markStack[ctx->markTop++] = ctx->opTop;
        break;
    case OP_UNMARK: // pop stack state set by mark
        ctx->markTop--;
        break;
    case OP_BREAK: // restore stack state (FOLLOW WITH JMP!)
        ctx->opTop = ctx->markStack[--ctx->markTop];
        break;
    default:
        ERR(ctx, "BUG: bad opcode");
    }

    if(ctx->fTop <= 0)
        ctx->done = 1;
}

static void nativeCall(struct Context* ctx, struct Frame* f, naRef ccode)
{
    naCFunction fptr = ccode.ref.ptr.ccode->fptr;
    naRef result = (*fptr)(ctx, f->args);
    ctx->fTop--;
    PUSH(ctx, result);
}

int naCurrentLine(struct Context* ctx)
{
    return ctx->fStack[ctx->fTop-1].line;
}

char* naGetError(struct Context* ctx)
{
    return ctx->error;
}

naRef naCall(struct Context* ctx, naRef code, naRef namespace)
{
    naRef func = naNewFunc(ctx, code, naNewClosure(ctx, namespace, naNil()));
    setupFuncall(ctx, func, naNewVector(ctx));
    
    // Return early if an error occurred.  It will be visible to the
    // caller via naGetError().
    if(setjmp(ctx->jumpHandle))
        return naNil();
    
    ctx->done = 0;
    while(!ctx->done) {
        struct Frame* f = &(ctx->fStack[ctx->fTop-1]);
        naRef code = f->func.ref.ptr.func->code;
        if(IS_CCODE(code)) nativeCall(ctx, f, code);
        else               run1(ctx, f, code);
        DBG(printStackDEBUG(ctx);)
    }
    DBG(printStackDEBUG(ctx);)

    return ctx->opStack[ctx->opTop-1];
}
