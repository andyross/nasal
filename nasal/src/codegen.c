#include <stdio.h> // DEBUG

#include "parse.h"
#include "code.h"

// These are more sensical predicate names in most contexts in this file
#define LEFT(tok)   ((tok)->children)
#define RIGHT(tok)  ((tok)->lastChild)
#define BINARY(tok) (LEFT(tok) && RIGHT(tok) && LEFT(tok) != RIGHT(tok))

// Forward references for recursion
static void genExpr(struct Parser* p, struct Token* t);
static void genExprList(struct Parser* p, struct Token* t);

static void emit(struct Parser* p, int byte)
{
    if(p->nBytes >= p->codeAlloced) {
        int i, sz = p->codeAlloced * 2;
        unsigned char* buf = naParseAlloc(p, sz);
        for(i=0; i<p->codeAlloced; i++) buf[i] = p->byteCode[i];
        p->byteCode = buf;
        p->codeAlloced = sz;
    }
    p->byteCode[p->nBytes++] = (unsigned char)byte;
}

static void emitImmediate(struct Parser* p, int byte, int arg)
{
    emit(p, byte);
    emit(p, arg >> 8);
    emit(p, arg & 0xff);
}

static void genBinOp(int op, struct Parser* p, struct Token* t)
{
    if(!LEFT(t) || !RIGHT(t))
        naParseError(p, "empty subexpression", t->line);
    genExpr(p, LEFT(t));
    genExpr(p, RIGHT(t));
    emit(p, op);
}

static int newConstant(struct Parser* p, naRef c)
{
    int i = p->nConsts++;
    if(i > 0xffff) naParseError(p, "too many constants in code block", 0);
    naHash_set(p->consts, naNum(i), c);
    return i;
}

static naRef getConstant(struct Parser* p, int idx)
{
    naRef c;
    naHash_get(p->consts, naNum(idx), &c);
    return c;
}

// Interns a scalar (!) constant and returns its index
static int internConstant(struct Parser* p, naRef c)
{
    naRef r;
    naHash_get(p->interned, c, &r);
    if(!IS_NIL(r)) {
        return (int)r.num;
    } else {
        int idx = newConstant(p, c);
        naHash_set(p->interned, c, naNum(idx));
        return idx;
    }
}

static void genScalarConstant(struct Parser* p, struct Token* t)
{
    naRef c;
    int idx;

    if(t->str) {
        c = naNewString(p->context);
        naStr_fromdata(c, t->str, t->strlen);
    } else {
        c = naNum(t->num);
    }

    idx = internConstant(p, c);
    emitImmediate(p, OP_PUSHCONST, idx);
}

static int genLValue(struct Parser* p, struct Token* t)
{
    if(t->type == TOK_LPAR) {
        return genLValue(p, LEFT(t)); // Handle stuff like "(a) = 1"
    } else if(t->type == TOK_SYMBOL) {
        genScalarConstant(p, t);
        return OP_SETLOCAL;
    } else if(t->type == TOK_DOT && RIGHT(t) && RIGHT(t)->type == TOK_SYMBOL) {
        genExpr(p, LEFT(t));
        genScalarConstant(p, RIGHT(t));
        return OP_SETMEMBER;
    } else if(t->type == TOK_LBRA) {
        genExpr(p, LEFT(t));
        genExpr(p, RIGHT(t));
        return OP_INSERT;
    } else {
        naParseError(p, "bad lvalue", t->line);
        return -1;
    }
}

// Hackish.  Save off the current lambda state and recurse
static void genLambda(struct Parser* p, struct Token* t)
{
    int idx;
    unsigned char* byteCode    = p->byteCode; 
    int            nBytes      = p->nBytes; 
    int            codeAlloced = p->codeAlloced; 
    naRef          consts      = p->consts; 
    naRef          interned    = p->interned; 
    int            nConsts     = p->nConsts; 

    if(LEFT(t)->type != TOK_LCURL)
        naParseError(p, "bad function definition", t->line);
    naRef codeObj = naCodeGen(p, LEFT(LEFT(t)));

    p->byteCode    = byteCode; 
    p->nBytes      = nBytes; 
    p->codeAlloced = codeAlloced; 
    p->consts      = consts; 
    p->interned    = interned; 
    p->nConsts     = nConsts; 

    idx = newConstant(p, codeObj);
    emitImmediate(p, OP_PUSHCONST, idx);
}

static void genList(struct Parser* p, struct Token* t)
{
    if(t->type == TOK_COMMA) {
        genExpr(p, LEFT(t));
        emit(p, OP_VAPPEND);
        genList(p, RIGHT(t));
    } else if(t->type == TOK_EMPTY) {
        return;
    } else {
        genExpr(p, t);
        emit(p, OP_VAPPEND);
    }
}

static void genHashElem(struct Parser* p, struct Token* t)
{
    if(t->type == TOK_EMPTY)
        return;
    if(t->type != TOK_COLON)
        naParseError(p, "bad hash/object initializer", t->line);
    if(LEFT(t)->type == TOK_SYMBOL) genScalarConstant(p, LEFT(t));
    else if(LEFT(t)->type == TOK_LITERAL) genExpr(p, LEFT(t));
    else naParseError(p, "bad hash/object initializer", t->line);
    genExpr(p, RIGHT(t));
    emit(p, OP_HAPPEND);
}

static void genHash(struct Parser* p, struct Token* t)
{
    if(t->type == TOK_COMMA) {
        genHashElem(p, LEFT(t));
        genHash(p, RIGHT(t));
    } else if(t->type != TOK_EMPTY) {
        genHashElem(p, t);
    }
}

static void genFuncall(struct Parser* p, struct Token* t)
{
    int op = OP_FCALL;
    if(LEFT(t)->type == TOK_DOT) {
        genExpr(p, LEFT(LEFT(t)));
        emit(p, OP_DUP);
        genScalarConstant(p, RIGHT(LEFT(t)));
        emit(p, OP_MEMBER);
        op = OP_MCALL;
    } else {
        genExpr(p, LEFT(t));
    }
    emit(p, OP_NEWVEC);
    if(RIGHT(t)) genList(p, RIGHT(t));
    emit(p, op);
}

// Emit a jump operation, and return the location of the address in
// the bytecode for future fixup in fixJumpTarget
static int emitJump(struct Parser* p, int op)
{
    int ip;
    emit(p, op);
    ip = p->nBytes;
    emit(p, 0xff); // dummy address
    emit(p, 0xff);
    return ip;
}

// Points a previous jump instruction at the current "end-of-bytecode"
static void fixJumpTarget(struct Parser* p, int spot)
{
    p->byteCode[spot]   = p->nBytes >> 8;
    p->byteCode[spot+1] = p->nBytes & 0xff;
}

static void genShortCircuit(struct Parser* p, struct Token* t)
{
    int jumpNext, jumpEnd, isAnd = (t->type == TOK_AND);
    genExpr(p, LEFT(t));
    if(isAnd) emit(p, OP_NOT);
    jumpNext = emitJump(p, OP_JIFNOT);
    emit(p, isAnd ? OP_PUSHNIL : OP_PUSHONE);
    jumpEnd = emitJump(p, OP_JMP);
    fixJumpTarget(p, jumpNext);
    genExpr(p, RIGHT(t));
    fixJumpTarget(p, jumpEnd);
}


static void genIf(struct Parser* p, struct Token* tif, struct Token* telse)
{
    int jumpNext, jumpEnd;
    genExpr(p, tif->children); // the test
    jumpNext = emitJump(p, OP_JIFNOT);
    genExprList(p, tif->children->next->children); // the body
    jumpEnd = emitJump(p, OP_JMP);
    fixJumpTarget(p, jumpNext);
    if(telse) {
        if(telse->type == TOK_ELSIF) genIf(p, telse, telse->next);
        else genExprList(p, telse->children->children);
    }
    fixJumpTarget(p, jumpEnd);
}

static void genIfElse(struct Parser* p, struct Token* t)
{
    genIf(p, t, t->children->next->next);
}

static int countSemis(struct Token* t)
{
    if(!t || t->type != TOK_SEMI) return 0;
    return 1 + countSemis(RIGHT(t));
}

static void genLoop(struct Parser* p, struct Token* body,
                    struct Token* update, int loopTop, int jumpEnd)
{
    int continueSpot;
    genExprList(p, body);
    emit(p, OP_POP);
    continueSpot = p->nBytes;
    if(update) { genExpr(p, update); emit(p, OP_POP); }
    emitImmediate(p, OP_JMP, loopTop);
    fixJumpTarget(p, jumpEnd);
    emit(p, OP_PUSHNIL); // Leave something on the stack
}

static void genForWhile(struct Parser* p, struct Token* init,
                        struct Token* test, struct Token* update,
                        struct Token* body)
{
    int loopTop, jumpEnd;

    if(init) { genExpr(p, init); emit(p, OP_POP); }

    loopTop = p->nBytes;
    genExpr(p, test);
    jumpEnd = emitJump(p, OP_JIFNOT);

    genLoop(p, body, update, loopTop, jumpEnd);
}

static void genWhile(struct Parser* p, struct Token* t)
{
    int semis = countSemis(LEFT(t));
    struct Token *test=LEFT(t), *body;
    if(semis == 1) test = RIGHT(LEFT(t)); // Handle label here
    else if(semis != 0)
        naParseError(p, "too many semicolons in while test", t->line);
    body = LEFT(RIGHT(t));
    genForWhile(p, 0, test, 0, body);
}

static void genFor(struct Parser* p, struct Token* t)
{
    struct Token *h, *init, *test, *body, *update;
    h = LEFT(t)->children;
    int semis = countSemis(h);
    if(semis == 3) h=RIGHT(h); // Handle label
    else if(semis != 2)
        naParseError(p, "wrong number of terms in for header", t->line);

    // Parse tree hell :)
    init = LEFT(h);
    test = LEFT(RIGHT(h));
    update = RIGHT(RIGHT(h));
    body = RIGHT(t)->children;
    genForWhile(p, init, test, update, body);
}

static void genForEach(struct Parser* p, struct Token* t)
{
    int loopTop, jumpEnd, assignOp;
    struct Token *h, *elem, *body, *vec;
    h = LEFT(LEFT(t));
    int semis = countSemis(h);
    if(semis == 2) h = RIGHT(h);
    else if (semis != 1)
        naParseError(p, "wrong number of terms in foreach header", t->line);
    elem = LEFT(h);
    vec = RIGHT(h);
    body = RIGHT(t)->children;

    genExpr(p, vec);
    emit(p, OP_PUSHZERO);
    loopTop = p->nBytes;
    emit(p, OP_EACH);
    jumpEnd = emitJump(p, OP_JIFNIL);
    assignOp = genLValue(p, elem);
    emit(p, OP_XCHG);
    emit(p, assignOp);

    genLoop(p, body, 0, loopTop, jumpEnd);
}

static void genExpr(struct Parser* p, struct Token* t)
{
    int i;
    if(t == 0)
        naParseError(p, "null subexpression", -1); // FIXME, ugly! No line!
    if(t->line != p->lastLine)
        emitImmediate(p, OP_LINE, t->line);
    p->lastLine = t->line;
    switch(t->type) {
    case TOK_IF:
        genIfElse(p, t);
        break;
    case TOK_WHILE:
        genWhile(p, t);
        break;
    case TOK_FOR:
        genFor(p, t);
        break;
    case TOK_FOREACH:
        genForEach(p, t);
        break;
    case TOK_BREAK: break;
    case TOK_CONTINUE: break;

    case TOK_TOP:
        genExprList(p, LEFT(t));
        break;
    case TOK_FUNC:
        genLambda(p, t);
        break;
    case TOK_LPAR:
        if(BINARY(t)) genFuncall(p, t);    // function invocation
        else          genExpr(p, LEFT(t)); // simple parenthesis
        break;
    case TOK_LBRA:
        if(BINARY(t)) {
            genBinOp(OP_EXTRACT, p, t); // a[i]
        } else {
            emit(p, OP_NEWVEC);
            genList(p, LEFT(t));
        }
        break;
    case TOK_LCURL:
        emit(p, OP_NEWHASH);
        genHash(p, LEFT(t));
        break;
    case TOK_ASSIGN:
        i = genLValue(p, LEFT(t));
        genExpr(p, RIGHT(t));
        emit(p, i); // use the op appropriate to the lvalue
        break;
    case TOK_RETURN:
        if(RIGHT(t)) genExpr(p, RIGHT(t));
        else emit(p, OP_PUSHNIL);
        emit(p, OP_RETURN);
        break;
    case TOK_NOT:
        genExpr(p, RIGHT(t));
        emit(p, OP_NOT);
        break;
    case TOK_SYMBOL:
        genScalarConstant(p, t);
        emit(p, OP_LOCAL);
        break;
    case TOK_LITERAL:
        genScalarConstant(p, t);
        break;
    case TOK_MINUS:
        if(BINARY(t)) {
            genBinOp(OP_MINUS,  p, t);  // binary subtraction
        } else if(RIGHT(t)->type == TOK_LITERAL && !RIGHT(t)->str) {
            RIGHT(t)->num *= -1;        // Pre-negate constants
            genScalarConstant(p, RIGHT(t));
        } else {
            genExpr(p, RIGHT(t));       // unary negation
            emit(p, OP_NEG);
        }
        break;
    case TOK_DOT:
        genExpr(p, LEFT(t));
        genScalarConstant(p, RIGHT(t));
        emit(p, OP_MEMBER);
        break;
    case TOK_EMPTY: case TOK_NIL:
        emit(p, OP_PUSHNIL); break; // *NOT* a noop!
    case TOK_AND: case TOK_OR:
        genShortCircuit(p, t);
        break;
    case TOK_MUL:   genBinOp(OP_MUL,    p, t); break;
    case TOK_PLUS:  genBinOp(OP_PLUS,   p, t); break;
    case TOK_DIV:   genBinOp(OP_DIV,    p, t); break;
    case TOK_CAT:   genBinOp(OP_CAT,    p, t); break;
    case TOK_LT:    genBinOp(OP_LT,     p, t); break;
    case TOK_LTE:   genBinOp(OP_LTE,    p, t); break;
    case TOK_EQ:    genBinOp(OP_EQ,     p, t); break;
    case TOK_NEQ:   genBinOp(OP_NEQ,    p, t); break;
    case TOK_GT:    genBinOp(OP_GT,     p, t); break;
    case TOK_GTE:   genBinOp(OP_GTE,    p, t); break;
    default:
        naParseError(p, "parse error", t->line);
    };
}

static void genExprList(struct Parser* p, struct Token* t)
{
    if(t->type == TOK_SEMI) {
        genExpr(p, t->children);
        if(t->lastChild && t->lastChild->type != TOK_EMPTY) {
            emit(p, OP_POP);
            genExprList(p, t->lastChild);
        }
    } else {
        genExpr(p, t);
    }
}

naRef naCodeGen(struct Parser* p, struct Token* t)
{
    int i;
    naRef codeObj;
    struct naCode* code;

    p->codeAlloced = 1024; // Start fairly big, this is a cheap allocation
    p->byteCode = naParseAlloc(p, p->codeAlloced);
    p->nBytes = 0;

    p->consts = naNewHash(p->context);
    p->interned = naNewHash(p->context);
    p->nConsts = 0;

    genExprList(p, t);

    // Now make a code object
    codeObj = naNewCode(p->context);
    code = codeObj.ref.ptr.code;
    code->nBytes = p->nBytes;
    code->byteCode = ALLOC(code->nBytes);
    for(i=0; i < code->nBytes; i++)
        code->byteCode[i] = p->byteCode[i];
    code->nConstants = p->nConsts;
    code->constants = ALLOC(code->nConstants * sizeof(naRef));
    for(i=0; i<code->nConstants; i++)
        code->constants[i] = getConstant(p, i);

    return codeObj;
}
