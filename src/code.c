#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "nasal.h"
#include "code.h"

////////////////////////////////////////////////////////////////////////
// Debugging stuff. ////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//#define INTERPRETER_DUMP
#if !defined(INTERPRETER_DUMP)
# define DBG(expr) /* noop */
#else
# define DBG(expr) expr
# include <stdio.h>
# include <stdlib.h>
#endif
char* opStringDEBUG(int op);
void printOpDEBUG(int ip, int op);
void printStackDEBUG(naContext ctx);
////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#define vsnprintf _vsnprintf
#endif

struct Globals* globals = 0;

static naRef bindFunction(naContext ctx, struct Frame* f, naRef code);

#define ERR(c, msg) naRuntimeError((c),(msg))
void naRuntimeError(naContext c, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(c->error, sizeof(c->error), fmt, ap);
    va_end(ap);
    longjmp(c->jumpHandle, 1);
}

void naRethrowError(naContext subc)
{
    strncpy(subc->callParent->error, subc->error, sizeof(subc->error));
    subc->callParent->dieArg = subc->dieArg;
    longjmp(subc->callParent->jumpHandle, 1);
}

#define END_PTR ((void*)1)
#define IS_END(r) (IS_REF((r)) && PTR((r)).obj == END_PTR)
static naRef endToken()
{
    naRef r;
    SETPTR(r, END_PTR);
    return r;
}

static int boolify(naContext ctx, naRef r)
{
    if(IS_NUM(r)) return r.num != 0;
    if(IS_NIL(r) || IS_END(r)) return 0;
    if(IS_STR(r)) {
        double d;
        if(naStr_len(r) == 0) return 0;
        if(naStr_tonum(r, &d)) return d != 0;
        else return 1;
    }
    ERR(ctx, "non-scalar used in boolean context");
    return 0;
}

static double numify(naContext ctx, naRef o)
{
    double n;
    if(IS_NUM(o)) return o.num;
    else if(IS_NIL(o)) ERR(ctx, "nil used in numeric context");
    else if(!IS_STR(o)) ERR(ctx, "non-scalar in numeric context");
    else if(naStr_tonum(o, &n)) return n;
    else ERR(ctx, "non-numeric string in numeric context");
    return 0;
}

static naRef stringify(naContext ctx, naRef r)
{
    if(IS_STR(r)) return r;
    if(IS_NUM(r)) return naStr_fromnum(naNewString(ctx), r.num);
    ERR(ctx, "non-scalar in string context");
    return naNil();
}

static int checkVec(naContext ctx, naRef vec, naRef idx)
{
    int i = (int)numify(ctx, idx);
    if(i < 0) i += naVec_size(vec);
    if(i < 0 || i >= naVec_size(vec))
        naRuntimeError(ctx, "vector index %d out of bounds (size: %d)",
                       i, naVec_size(vec));
    return i;
}

static int checkStr(naContext ctx, naRef str, naRef idx)
{
    int i = (int)numify(ctx, idx);
    if(i < 0) i += naStr_len(str);
    if(i < 0 || i >= naStr_len(str))
        naRuntimeError(ctx, "string index %d out of bounds (size: %d)",
                       i, naStr_len(str));
    return i;
}

static naRef containerGet(naContext ctx, naRef box, naRef key)
{
    naRef result = naNil();
    if(!IS_SCALAR(key)) ERR(ctx, "container index not scalar");
    if(IS_HASH(box))
        naHash_get(box, key, &result);
    else if(IS_VEC(box))
        result = naVec_get(box, checkVec(ctx, box, key));
    else if(IS_STR(box))
        result = naNum((unsigned char)naStr_data(box)[checkStr(ctx, box, key)]);
    else
        ERR(ctx, "extract from non-container");
    return result;
}

static void containerSet(naContext ctx, naRef box, naRef key, naRef val)
{
    if(!IS_SCALAR(key))   ERR(ctx, "container index not scalar");
    else if(IS_HASH(box)) naHash_set(box, key, val);
    else if(IS_VEC(box))  naVec_set(box, checkVec(ctx, box, key), val);
    else if(IS_STR(box)) {
        if(PTR(box).str->hashcode)
            ERR(ctx, "cannot change immutable string");
        naStr_data(box)[checkStr(ctx, box, key)] = (char)numify(ctx, val);
    } else ERR(ctx, "insert into non-container");
}

static void initTemps(naContext c)
{
    c->tempsz = 4;
    c->temps = naAlloc(c->tempsz * sizeof(struct naObj*));
    c->ntemps = 0;
}

static void initContext(naContext c)
{
    int i;
    c->fTop = c->opTop = c->markTop = 0;
    for(i=0; i<NUM_NASAL_TYPES; i++)
        c->nfree[i] = 0;

    if(c->tempsz > 32) {
        naFree(c->temps);
        initTemps(c);
    }

    c->callParent = 0;
    c->callChild = 0;
    c->dieArg = naNil();
    c->error[0] = 0;
    c->userData = 0;
}

static void initGlobals()
{
    int i;
    naContext c;
    globals = (struct Globals*)naAlloc(sizeof(struct Globals));
    naBZero(globals, sizeof(struct Globals));

    globals->sem = naNewSem();
    globals->lock = naNewLock();

    globals->allocCount = 256; // reasonable starting value
    for(i=0; i<NUM_NASAL_TYPES; i++)
        naGC_init(&(globals->pools[i]), i);
    globals->deadsz = 256;
    globals->ndead = 0;
    globals->deadBlocks = naAlloc(sizeof(void*) * globals->deadsz);

    // Initialize a single context
    globals->freeContexts = 0;
    globals->allContexts = 0;
    c = naNewContext();

    globals->symbols = naNewHash(c);
    globals->save = naNewVector(c);

    // Cache pre-calculated "me", "arg" and "parents" scalars
    globals->meRef = naInternSymbol(naStr_fromdata(naNewString(c), "me", 2));
    globals->argRef = naInternSymbol(naStr_fromdata(naNewString(c), "arg", 3));
    globals->parentsRef = naInternSymbol(naStr_fromdata(naNewString(c), "parents", 7));

    naFreeContext(c);
}

naContext naNewContext()
{
    naContext c;
    if(globals == 0)
        initGlobals();

    LOCK();
    c = globals->freeContexts;
    if(c) {
        globals->freeContexts = c->nextFree;
        c->nextFree = 0;
        UNLOCK();
        initContext(c);
    } else {
        UNLOCK();
        c = (naContext)naAlloc(sizeof(struct Context));
        initTemps(c);
        initContext(c);
        LOCK();
        c->nextAll = globals->allContexts;
        c->nextFree = 0;
        globals->allContexts = c;
        UNLOCK();
    }
    return c;
}

naContext naSubContext(naContext super)
{
    naContext ctx = naNewContext();
    if(super->callChild) naFreeContext(super->callChild);
    ctx->callParent = super;
    super->callChild = ctx;
    return ctx;
}

void naFreeContext(naContext c)
{
    c->ntemps = 0;
    if(c->callChild) naFreeContext(c->callChild);
    if(c->callParent) c->callParent->callChild = 0;
    LOCK();
    c->nextFree = globals->freeContexts;
    globals->freeContexts = c;
    UNLOCK();
}

// Note that opTop is incremented separately, to avoid situations
// where the "r" expression also references opTop.  The SGI compiler
// is known to have issues with such code.
#define PUSH(r) do { \
    if(ctx->opTop >= MAX_STACK_DEPTH) ERR(ctx, "stack overflow"); \
    ctx->opStack[ctx->opTop] = r; \
    ctx->opTop++;                 \
    } while(0)

static void setupArgs(naContext ctx, struct Frame* f, naRef* args, int nargs)
{
    int i;
    struct naCode* c = PTR(PTR(f->func).func->code).code;

    // Set the argument symbols, and put any remaining args in a vector
    if(nargs < c->nArgs)
        naRuntimeError(ctx, "too few function args (have %d need %d)",
            nargs, c->nArgs);
    for(i=0; i<c->nArgs; i++)
        naiHash_newsym(PTR(f->locals).hash,
                      &c->constants[ARGSYMS(c)[i]], &args[i]);
    args += c->nArgs;
    nargs -= c->nArgs;
    for(i=0; i<c->nOptArgs; i++, nargs--) {
        naRef val = nargs > 0 ? args[i] : c->constants[OPTARGVALS(c)[i]];
        if(IS_CODE(val))
            val = bindFunction(ctx, &ctx->fStack[ctx->fTop-2], val);
        naiHash_newsym(PTR(f->locals).hash, &c->constants[OPTARGSYMS(c)[i]], 
                      &val);
    }
    args += c->nOptArgs;
    if(c->needArgVector || nargs > 0) {
        naRef argv = naNewVector(ctx);
        naVec_setsize(argv, nargs > 0 ? nargs : 0);
        for(i=0; i<nargs; i++)
            PTR(argv).vec->rec->array[i] = *args++;
        naiHash_newsym(PTR(f->locals).hash, &c->constants[c->restArgSym], &argv);
    }
}

static void checkNamedArgs(naContext ctx, struct naCode* c, struct naHash* h)
{
    int i;
    naRef sym, rest, dummy;
    for(i=0; i<c->nArgs; i++) {
        sym = c->constants[ARGSYMS(c)[i]];
        if(!naiHash_sym(h, PTR(sym).str, &dummy))
            naRuntimeError(ctx, "Missing arg: %s", naStr_data(sym));
    }
    for(i=0; i<c->nOptArgs; i++) {
        sym = c->constants[OPTARGSYMS(c)[i]];
        if(!naiHash_sym(h, PTR(sym).str, &dummy))
            naiHash_newsym(h, &sym, &c->constants[OPTARGVALS(c)[i]]);
    }
    if(c->needArgVector) {
        sym = c->constants[c->restArgSym];
        if(!naiHash_sym(h, PTR(sym).str, &dummy)) {
            rest = naNewVector(ctx);
            naiHash_newsym(h, &sym, &rest);
        }
    }
}

static struct Frame* setupFuncall(naContext ctx, int nargs, int mcall, int named)
{
    naRef *args, func, code, obj = naNil();
    struct Frame* f;
    int opf = ctx->opTop - nargs;

    args = &ctx->opStack[opf];
    func = ctx->opStack[--opf];
    if(!IS_FUNC(func)) ERR(ctx, "function/method call on uncallable object");
    code = PTR(func).func->code;
    if(mcall) obj = ctx->opStack[--opf];
    ctx->opFrame = opf;

    if(IS_CCODE(code)) {
        naRef result = (*PTR(code).ccode->fptr)(ctx, obj, nargs, args);
        if(named) ERR(ctx, "native functions have no named arguments");
        ctx->opTop = ctx->opFrame;
        PUSH(result);
        return &(ctx->fStack[ctx->fTop-1]);
    }
    
    if(ctx->fTop >= MAX_RECURSION) ERR(ctx, "call stack overflow");
    
    f = &(ctx->fStack[ctx->fTop]);
    f->locals = named ? args[0] : naNewHash(ctx);
    f->func = func;
    f->ip = 0;
    f->bp = ctx->opFrame;

    if(mcall) naHash_set(f->locals, globals->meRef, obj);

    if(named) checkNamedArgs(ctx, PTR(code).code, PTR(f->locals).hash);
    else      setupArgs(ctx, f, args, nargs);

    ctx->fTop++;
    ctx->opTop = f->bp; /* Pop the stack last, to avoid GC lossage */
    return f;
}

static naRef evalEquality(int op, naRef ra, naRef rb)
{
    int result = naEqual(ra, rb);
    return naNum((op==OP_EQ) ? result : !result);
}

static naRef evalCat(naContext ctx, naRef l, naRef r)
{
    if(IS_VEC(l) && IS_VEC(r)) {
        int i, ls = naVec_size(l), rs = naVec_size(r);
        naRef v = naNewVector(ctx);
        naVec_setsize(v, ls + rs);
        for(i=0; i<ls; i+=1) naVec_set(v, i, naVec_get(l, i));
        for(i=0; i<rs; i+=1) naVec_set(v, i+ls, naVec_get(r, i));
        return v;
    } else {
        naRef a = stringify(ctx, l);
        naRef b = stringify(ctx, r);
        return naStr_concat(naNewString(ctx), a, b);
    }
}

// When a code object comes out of the constant pool and shows up on
// the stack, it needs to be bound with the lexical context.
static naRef bindFunction(naContext ctx, struct Frame* f, naRef code)
{
    naRef result = naNewFunc(ctx, code);
    PTR(result).func->namespace = f->locals;
    PTR(result).func->next = f->func;
    return result;
}

static int getClosure(struct naFunc* c, naRef sym, naRef* result)
{
    while(c) {
        if(naHash_get(c->namespace, sym, result)) return 1;
        c = PTR(c->next).func;
    }
    return 0;
}

static naRef getLocal2(naContext ctx, struct Frame* f, naRef sym)
{
    naRef result;
    if(!naHash_get(f->locals, sym, &result))
        if(!getClosure(PTR(f->func).func, sym, &result))
            naRuntimeError(ctx, "undefined symbol: %s", naStr_data(sym));
    return result;
}

static void getLocal(naContext ctx, struct Frame* f, naRef* sym, naRef* out)
{
    struct naFunc* func;
    struct naStr* str = PTR(*sym).str;
    if(naiHash_sym(PTR(f->locals).hash, str, out))
        return;
    func = PTR(f->func).func;
    while(func && PTR(func->namespace).hash) {
        if(naiHash_sym(PTR(func->namespace).hash, str, out))
            return;
        func = PTR(func->next).func;
    }
    // Now do it again using the more general naHash_get().  This will
    // only be necessary if something has created the value in the
    // namespace using the more generic hash syntax
    // (e.g. namespace["symbol"] and not namespace.symbol).
    *out = getLocal2(ctx, f, *sym);
}

static int setClosure(naRef func, naRef sym, naRef val)
{
    struct naFunc* c = PTR(func).func;
    if(c == 0) return 0;
    if(naiHash_tryset(c->namespace, sym, val)) return 1;
    return setClosure(c->next, sym, val);
}

static void setSymbol(struct Frame* f, naRef sym, naRef val)
{
    // Try the locals first, if not already there try the closures in
    // order.  Finally put it in the locals if nothing matched.
    if(!naiHash_tryset(f->locals, sym, val))
        if(!setClosure(f->func, sym, val))
            naHash_set(f->locals, sym, val);
}

// Funky API: returns null to indicate no member, an empty string to
// indicate success, or a non-empty error message.  Works this way so
// we can generate smart error messages without throwing them with a
// longjmp -- this gets called under naMember_get() from C code.
static const char* getMember_r(naRef obj, naRef field, naRef* out, int count)
{
    int i;
    naRef p;
    struct VecRec* pv;
    if(--count < 0) return "too many parents";
    if(!IS_HASH(obj)) return "non-objects have no members";
    if(naHash_get(obj, field, out)) return "";
    if(!naHash_get(obj, globals->parentsRef, &p)) return 0;
    if(!IS_VEC(p)) return "object \"parents\" field not vector";
    pv = PTR(p).vec->rec;
    for(i=0; pv && i<pv->size; i++) {
        const char* err = getMember_r(pv->array[i], field, out, count);
        if(err) return err; /* either an error or success */
    }
    return 0;
}

static void getMember(naContext ctx, naRef obj, naRef fld,
                      naRef* result, int count)
{
    const char* err = getMember_r(obj, fld, result, count);
    if(!err)   naRuntimeError(ctx, "No such member: %s", naStr_data(fld));
    if(err[0]) naRuntimeError(ctx, err);
}

int naMember_get(naRef obj, naRef field, naRef* out)
{
    const char* err = getMember_r(obj, field, out, 64);
    return err && !err[0];
}

// OP_EACH works like a vector get, except that it leaves the vector
// and index on the stack, increments the index after use, and
// pushes a nil if the index is beyond the end.
static void evalEach(naContext ctx, int useIndex)
{
    int idx = (int)(ctx->opStack[ctx->opTop-1].num);
    naRef vec = ctx->opStack[ctx->opTop-2];
    if(!IS_VEC(vec)) ERR(ctx, "foreach enumeration of non-vector");
    if(!PTR(vec).vec->rec || idx >= PTR(vec).vec->rec->size) {
        PUSH(endToken());
        return;
    }
    ctx->opStack[ctx->opTop-1].num = idx+1; // modify in place
    PUSH(useIndex ? naNum(idx) : naVec_get(vec, idx));
}

static void evalUnpack(naContext ctx, int count)
{
    naRef vec = ctx->opStack[--ctx->opTop];
    if(!IS_VEC(vec) || naVec_size(vec) < count)
        ERR(ctx, "short or invalid multi-assignment vector");
    while(count--) PUSH(naVec_get(vec, count));
}

// FIXME: unify with almost identical checkVec() above
static int vbound(naContext ctx, naRef v, naRef ir, int end)
{
    int sz=naVec_size(v), i = IS_NIL(ir) ? (end ? -1 : 0) : numify(ctx, ir);
    if(IS_NIL(ir) && !sz) return i;
    if(i < 0) i += sz;
    if(i < 0 || i >= sz)
        naRuntimeError(ctx, "slice index %d out of bounds (size: %d)",
                       i, sz);
    return i;
}

static void evalSlice(naContext ctx, naRef src, naRef dst, naRef idx)
{
    if(!IS_VEC(src)) ERR(ctx, "cannot slice non-vector");
    naVec_append(dst, naVec_get(src, checkVec(ctx, src, idx)));
}
 
static void evalSlice2(naContext ctx, naRef src, naRef dst,
                       naRef start, naRef endr)
{
    int i, end;
    if(!IS_VEC(src)) ERR(ctx, "cannot slice non-vector");
    end = vbound(ctx, src, endr, 1);
    for(i = vbound(ctx, src, start, 0); i<=end; i++)
        naVec_append(dst, naVec_get(src, i));
}

#define ARG() BYTECODE(cd)[f->ip++]
#define CONSTARG() cd->constants[ARG()]
#define POP() ctx->opStack[--ctx->opTop]
#define STK(n) (ctx->opStack[ctx->opTop-(n)])
#define SETFRAME(F) f = (F); cd = PTR(PTR(f->func).func->code).code;
#define FIXFRAME() SETFRAME(&(ctx->fStack[ctx->fTop-1]))
static naRef run(naContext ctx)
{
    struct Frame* f;
    struct naCode* cd;
    int op, arg;
    naRef a, b;

    ctx->dieArg = naNil();
    ctx->error[0] = 0;

    FIXFRAME();

    while(1) {
        op = BYTECODE(cd)[f->ip++];
        DBG(printf("Stack Depth: %d\n", ctx->opTop));
        DBG(printOpDEBUG(f->ip-1, op));
        switch(op) {
        case OP_POP:  ctx->opTop--; break;
        case OP_DUP:  PUSH(STK(1)); break;
        case OP_DUP2: PUSH(STK(2)); PUSH(STK(2)); break;
        case OP_XCHG:  a=STK(1); STK(1)=STK(2); STK(2)=a; break;
        case OP_XCHG2: a=STK(1); STK(1)=STK(2); STK(2)=STK(3); STK(3)=a; break;

#define BINOP(expr) do { \
    double l = IS_NUM(STK(2)) ? STK(2).num : numify(ctx, STK(2)); \
    double r = IS_NUM(STK(1)) ? STK(1).num : numify(ctx, STK(1)); \
    SETNUM(STK(2), expr);                                         \
    ctx->opTop--; } while(0)

        case OP_PLUS:  BINOP(l + r);         break;
        case OP_MINUS: BINOP(l - r);         break;
        case OP_MUL:   BINOP(l * r);         break;
        case OP_DIV:   BINOP(l / r);         break;
        case OP_LT:    BINOP(l <  r ? 1 : 0); break;
        case OP_LTE:   BINOP(l <= r ? 1 : 0); break;
        case OP_GT:    BINOP(l >  r ? 1 : 0); break;
        case OP_GTE:   BINOP(l >= r ? 1 : 0); break;
#undef BINOP

        case OP_EQ: case OP_NEQ:
            STK(2) = evalEquality(op, STK(2), STK(1));
            ctx->opTop--;
            break;
        case OP_CAT:
            STK(2) = evalCat(ctx, STK(2), STK(1));
            ctx->opTop--;
            break;
        case OP_NEG:
            STK(1) = naNum(-numify(ctx, STK(1)));
            break;
        case OP_NOT:
            STK(1) = naNum(boolify(ctx, STK(1)) ? 0 : 1);
            break;
        case OP_PUSHCONST:
            a = CONSTARG();
            if(IS_CODE(a)) a = bindFunction(ctx, f, a);
            PUSH(a);
            break;
        case OP_PUSHONE:
            PUSH(naNum(1));
            break;
        case OP_PUSHZERO:
            PUSH(naNum(0));
            break;
        case OP_PUSHNIL:
            PUSH(naNil());
            break;
        case OP_PUSHEND:
            PUSH(endToken());
            break;
        case OP_NEWVEC:
            PUSH(naNewVector(ctx));
            break;
        case OP_VAPPEND:
            naVec_append(STK(2), STK(1));
            ctx->opTop--;
            break;
        case OP_NEWHASH:
            PUSH(naNewHash(ctx));
            break;
        case OP_HAPPEND:
            naHash_set(STK(3), STK(2), STK(1));
            ctx->opTop -= 2;
            break;
        case OP_LOCAL:
            a = CONSTARG();
            getLocal(ctx, f, &a, &b);
            PUSH(b);
            break;
        case OP_SETSYM:
            setSymbol(f, STK(1), STK(2));
            ctx->opTop--;
            break;
        case OP_SETLOCAL:
            naHash_set(f->locals, STK(1), STK(2));
            ctx->opTop--;
            break;
        case OP_MEMBER:
            getMember(ctx, STK(1), CONSTARG(), &STK(1), 64);
            break;
        case OP_SETMEMBER:
            if(!IS_HASH(STK(2))) ERR(ctx, "non-objects have no members");
            naHash_set(STK(2), STK(1), STK(3));
            ctx->opTop -= 2;
            break;
        case OP_INSERT:
            containerSet(ctx, STK(2), STK(1), STK(3));
            ctx->opTop -= 2;
            break;
        case OP_EXTRACT:
            STK(2) = containerGet(ctx, STK(2), STK(1));
            ctx->opTop--;
            break;
        case OP_SLICE:
            evalSlice(ctx, STK(3), STK(2), STK(1));
            ctx->opTop--;
            break;
        case OP_SLICE2:
            evalSlice2(ctx, STK(4), STK(3), STK(2), STK(1));
            ctx->opTop -= 2;
            break;
        case OP_JMPLOOP:
            // Identical to JMP, except for locking
            naCheckBottleneck();
            f->ip = BYTECODE(cd)[f->ip];
            DBG(printf("   [Jump to: %d]\n", f->ip));
            break;
        case OP_JMP:
            f->ip = BYTECODE(cd)[f->ip];
            DBG(printf("   [Jump to: %d]\n", f->ip));
            break;
        case OP_JIFEND:
            arg = ARG();
            if(IS_END(STK(1))) {
                ctx->opTop--; // Pops **ONLY** if it's nil!
                f->ip = arg;
                DBG(printf("   [Jump to: %d]\n", f->ip));
            }
            break;
        case OP_JIFTRUE:
            arg = ARG();
            if(boolify(ctx, STK(1))) {
                f->ip = arg;
                DBG(printf("   [Jump to: %d]\n", f->ip));
            }
            break;
        case OP_JIFNOT:
            arg = ARG();
            if(!boolify(ctx, STK(1))) {
                f->ip = arg;
                DBG(printf("   [Jump to: %d]\n", f->ip));
            }
            break;
        case OP_JIFNOTPOP:
            arg = ARG();
            if(!boolify(ctx, POP())) {
                f->ip = arg;
                DBG(printf("   [Jump to: %d]\n", f->ip));
            }
            break;
        case OP_FCALL:  SETFRAME(setupFuncall(ctx, ARG(), 0, 0)); break;
        case OP_MCALL:  SETFRAME(setupFuncall(ctx, ARG(), 1, 0)); break;
        case OP_FCALLH: SETFRAME(setupFuncall(ctx,     1, 0, 1)); break;
        case OP_MCALLH: SETFRAME(setupFuncall(ctx,     1, 1, 1)); break;
        case OP_RETURN:
            a = STK(1);
            ctx->dieArg = naNil();
            if(ctx->callChild) naFreeContext(ctx->callChild);
            if(--ctx->fTop <= 0) return a;
            ctx->opTop = f->bp + 1; // restore the correct opstack frame!
            STK(1) = a;
            FIXFRAME();
            break;
        case OP_EACH:
            evalEach(ctx, 0);
            break;
        case OP_INDEX:
            evalEach(ctx, 1);
            break;
        case OP_MARK: // save stack state (e.g. "setjmp")
            if(ctx->markTop >= MAX_MARK_DEPTH)
                ERR(ctx, "mark stack overflow");
            ctx->markStack[ctx->markTop++] = ctx->opTop;
            break;
        case OP_UNMARK: // pop stack state set by mark
            ctx->markTop--;
            break;
        case OP_BREAK: // restore stack state (FOLLOW WITH JMP!)
            ctx->opTop = ctx->markStack[ctx->markTop-1];
            break;
        case OP_BREAK2: // same, but also pop the mark stack
            ctx->opTop = ctx->markStack[--ctx->markTop];
            break;
        case OP_UNPACK:
            evalUnpack(ctx, ARG());
            break;
        default:
            ERR(ctx, "BUG: bad opcode");
        }
        ctx->ntemps = 0; // reset GC temp vector
        DBG(printStackDEBUG(ctx));
    }
    return naNil(); // unreachable
}
#undef POP
#undef CONSTARG
#undef STK
#undef FIXFRAME

void naSave(naContext ctx, naRef obj)
{
    naVec_append(globals->save, obj);
}

int naStackDepth(naContext ctx)
{
    return ctx ? ctx->fTop + naStackDepth(ctx->callChild): 0;
}

static int findFrame(naContext ctx, naContext* out, int fn)
{
    int sd = naStackDepth(ctx->callChild);
    if(fn < sd) return findFrame(ctx->callChild, out, fn);
    *out = ctx;
    return ctx->fTop - 1 - (fn - sd);
}

int naGetLine(naContext ctx, int frame)
{
    struct Frame* f;
    frame = findFrame(ctx, &ctx, frame);
    f = &ctx->fStack[frame];
    if(IS_FUNC(f->func) && IS_CODE(PTR(f->func).func->code)) {
        struct naCode* c = PTR(PTR(f->func).func->code).code;
        unsigned short* p = LINEIPS(c) + c->nLines - 2;
        while(p >= LINEIPS(c) && p[0] > f->ip)
            p -= 2;
        return p[1];
    }
    return -1;
}

naRef naGetSourceFile(naContext ctx, int frame)
{
    naRef f;
    frame = findFrame(ctx, &ctx, frame);
    f = ctx->fStack[frame].func;
    f = PTR(f).func->code;
    return PTR(f).code->srcFile;
}

char* naGetError(naContext ctx)
{
    if(IS_STR(ctx->dieArg))
        return naStr_data(ctx->dieArg);
    return ctx->error[0] ? ctx->error : 0;
}

naRef naBindFunction(naContext ctx, naRef code, naRef closure)
{
    naRef func = naNewFunc(ctx, code);
    PTR(func).func->namespace = closure;
    PTR(func).func->next = naNil();
    return func;
}

naRef naBindToContext(naContext ctx, naRef code)
{
    naRef func = naNewFunc(ctx, code);
    if(ctx->fTop) {
        struct Frame* f = &ctx->fStack[ctx->fTop-1];
        PTR(func).func->namespace = f->locals;
        PTR(func).func->next = f->func;
    }
    return func;
}

naRef naCall(naContext ctx, naRef func, int argc, naRef* args,
             naRef obj, naRef locals)
{
    int i;
    naRef result;
    if(!ctx->callParent) naModLock();

    // We might have to allocate objects, which can call the GC.  But
    // the call isn't on the Nasal stack yet, so the GC won't find our
    // C-space arguments.
    naTempSave(ctx, func);
    for(i=0; i<argc; i++)
        naTempSave(ctx, args[i]);
    naTempSave(ctx, obj);
    naTempSave(ctx, locals);

    // naRuntimeError() calls end up here:
    if(setjmp(ctx->jumpHandle)) {
        if(!ctx->callParent) naModUnlock();
        return naNil();
    }

    if(IS_CCODE(PTR(func).func->code)) {
        naCFunction fp = PTR(PTR(func).func->code).ccode->fptr;
        result = (*fp)(ctx, obj, argc, args);
        if(!ctx->callParent) naModUnlock();
        return result;
    }

    if(IS_NIL(locals))
        locals = naNewHash(ctx);
    if(!IS_FUNC(func)) {
        func = naNewFunc(ctx, func);
        PTR(func).func->namespace = locals;
    }
    if(!IS_NIL(obj))
        naHash_set(locals, globals->meRef, obj);

    ctx->opTop = ctx->markTop = 0;
    ctx->fTop = 1;
    ctx->fStack[0].func = func;

    ctx->fStack[0].locals = locals;
    ctx->fStack[0].ip = 0;
    ctx->fStack[0].bp = ctx->opTop;

    setupArgs(ctx, ctx->fStack, args, argc);

    result = run(ctx);
    if(!ctx->callParent) naModUnlock();
    return result;
}

naRef naContinue(naContext ctx)
{
    naRef result;
    if(!ctx->callParent) naModLock();

    ctx->dieArg = naNil();
    ctx->error[0] = 0;

    if(setjmp(ctx->jumpHandle)) {
        if(!ctx->callParent) naModUnlock();
        else naRethrowError(ctx);
        return naNil();
    }

    // Wipe off the old function arguments, and push the expected
    // result (either the result of our subcontext, or a synthesized
    // nil if the thrown error was from an extension function or
    // in-script die() call) before re-running the code from the
    // instruction following the error.
    ctx->opTop = ctx->opFrame;
    PUSH(ctx->callChild ? naContinue(ctx->callChild) : naNil());

    // Getting here means the child completed successfully.  But
    // because its original C stack was longjmp'd out of existence,
    // there is no one left to free the context, so we have to do it.
    // This is fragile, but unfortunately required.
    if(ctx->callChild) naFreeContext(ctx->callChild);

    result = run(ctx);
    if(!ctx->callParent) naModUnlock();
    return result;
}
