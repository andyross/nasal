#include <stdio.h> // DEBUG

#include "parse.h"
#include "code.h"

// These are more sensical predicate names in most contexts in this file
#define LEFT(tok)   ((tok)->children)
#define RIGHT(tok)  ((tok)->lastChild)
#define BINARY(tok) (LEFT(tok) != RIGHT(tok))

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

static void genBinOp(int op, struct Parser* p, struct Token* t)
{
    genExpr(p, LEFT(t));
    genExpr(p, RIGHT(t));
    emit(p, op);
}

static int newConstant(struct Parser* p, naRef c)
{
    int i = p->nConsts++;
    if(i > 0xffff) naParseError(p, "Too many constants in code block", 0);
    naHash_set(p->consts, naNum(i), c);
    return i;
}

static naRef getConstant(struct Parser* p, int idx)
{
    return naHash_get(p->consts, naNum(idx));
}

// Interns a scalar (!) constant and returns its index
static int internConstant(struct Parser* p, naRef c)
{
    naRef r = naHash_get(p->interned, c);
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
    emit(p, OP_PUSHCONST);
    emit(p, idx >> 8);
    emit(p, idx & 0xff);
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

    if(LEFT(t)->type != TOK_LCURL) ERR("bad function definition");
    naRef codeObj = naCodeGen(p, LEFT(LEFT(t)));

    p->byteCode    = byteCode;
    p->nBytes      = nBytes;
    p->codeAlloced = codeAlloced;
    p->consts      = consts;
    p->interned    = interned;
    p->nConsts     = nConsts;

    idx = newConstant(p, codeObj);
    emit(p, OP_PUSHCONST);
    emit(p, idx >> 8);
    emit(p, idx & 0xff);
}

static void genList(struct Parser* p, struct Token* t)
{
    if(t->type == TOK_COMMA) {
        genExpr(p, LEFT(t));
        emit(p, OP_VAPPEND);
        genList(p, RIGHT(t));
    } else {
        genExpr(p, t);
        emit(p, OP_VAPPEND);
    }
}

static void genHashElem(struct Parser* p, struct Token* t)
{
    if(t->type != TOK_COLON)
        naParseError(p, "bad hash/object initializer", t->line);
    genExpr(p, LEFT(t));
    genExpr(p, RIGHT(t));
    emit(p, OP_HAPPEND);
}

static void genHash(struct Parser* p, struct Token* t)
{
    if(t->type == TOK_COMMA) {
        genHashElem(p, LEFT(t));
        genHash(p, RIGHT(t));
    } else {
        genHashElem(p, t);
    }
}

static void genFuncall(struct Parser* p, struct Token* t)
{
    *(int*)0=0;
}

static void genExpr(struct Parser* p, struct Token* t)
{
    int i;

    switch(t->type) {
    case TOK_IF: break;
    case TOK_FOR: break;
    case TOK_FOREACH: break;
    case TOK_WHILE: break;
    case TOK_BREAK: break;
    case TOK_CONTINUE: break;

    case TOK_TOP: genExprList(p, LEFT(t)); break;
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
        genExpr(p, LEFT(t));
        emit(p, OP_RETURN);
        break;
    case TOK_NOT:
        genExpr(p, LEFT(t));
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
    case TOK_AND: // FIXME: short-circuit
        genBinOp(OP_AND,    p, t); break;
    case TOK_OR:  // FIXME: short-circuit
        genBinOp(OP_OR,     p, t); break;
    case TOK_EMPTY: emit(p, OP_PUSHNIL); break; // *NOT* a noop!
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
        emit(p, OP_POP);
        genExprList(p, t->lastChild);
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
