#include <string.h>
#include "parse.h"
#include "code.h"

#define MAX_FUNARGS 32

// These are more sensical predicate names in most contexts in this file
#define LEFT(tok)   ((tok)->children)
#define RIGHT(tok)  ((tok)->lastChild)
#define BINARY(tok) (LEFT(tok) && RIGHT(tok) && LEFT(tok)->next == RIGHT(tok))

// Forward references for recursion
static void genExpr(struct Parser* p, struct Token* t);
static void genExprList(struct Parser* p, struct Token* t);
static naRef newLambda(struct Parser* p, struct Token* t);

static void emit(struct Parser* p, int val)
{
    if(p->cg->codesz >= p->cg->codeAlloced) {
        int i, sz = p->cg->codeAlloced * 2;
        unsigned short* buf = naParseAlloc(p, sz*sizeof(unsigned short));
        for(i=0; i<p->cg->codeAlloced; i++) buf[i] = p->cg->byteCode[i];
        p->cg->byteCode = buf;
        p->cg->codeAlloced = sz;
    }
    p->cg->byteCode[p->cg->codesz++] = (unsigned short)val;
}

static void emitImmediate(struct Parser* p, int val, int arg)
{
    emit(p, val);
    emit(p, arg);
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
    int i;
    naVec_append(p->cg->consts, c);
    i = naVec_size(p->cg->consts) - 1;
    if(i > 0xffff) naParseError(p, "too many constants in code block", 0);
   return i;
}

// Interns a scalar (!) constant and returns its index
static int internConstant(struct Parser* p, naRef c)
{
    int i, n = naVec_size(p->cg->consts);
    if(IS_CODE(c)) return newConstant(p, c);
    for(i=0; i<n; i++) {
        naRef b = naVec_get(p->cg->consts, i);
        if(IS_NUM(b) && IS_NUM(c) && b.num == c.num) return i;
        else if(IS_NIL(b) && IS_NIL(c)) return i;
        else if(naStrEqual(b, c)) return i;
    }
    return newConstant(p, c);
}

/* FIXME: this API is fundamentally a resource leak, because symbols
 * can't be deregistered.  The "proper" way to do this would be to
 * keep a reference count for each symbol, and decrement it when a
 * code object referencing it is deleted. */
naRef naInternSymbol(naRef sym)
{
    naRef result;
    if(naHash_get(globals->symbols, sym, &result))
        return result;
    naHash_set(globals->symbols, sym, sym);
    return sym;
}

static int findConstantIndex(struct Parser* p, struct Token* t)
{
    naRef c, dummy;
    if(t->type == TOK_NIL) c = naNil();
    else if(t->str) {
        c = naStr_fromdata(naNewString(p->context), t->str, t->strlen);
        naHash_get(globals->symbols, c, &dummy); // noop, make c immutable
        if(t->type == TOK_SYMBOL) c = naInternSymbol(c);
    } else if(t->type == TOK_FUNC) c = newLambda(p, t);
    else if(t->type == TOK_LITERAL) c = naNum(t->num);
    else naParseError(p, "invalid/non-constant constant", t->line);
    return internConstant(p, c);
}

static int genScalarConstant(struct Parser* p, struct Token* t)
{
    int idx;
    if(t->str == 0 && t->num == 1) { emit(p, OP_PUSHONE); return 0; }
    if(t->str == 0 && t->num == 0) { emit(p, OP_PUSHZERO); return 0; }
    emitImmediate(p, OP_PUSHCONST, idx = findConstantIndex(p, t));
    return idx;
}

static int genLValue(struct Parser* p, struct Token* t, int* cidx)
{
    if(!t) naParseError(p, "bad lvalue", -1);
    if(t->type == TOK_LPAR && t->rule != PREC_SUFFIX) {
        return genLValue(p, LEFT(t), cidx); // Handle stuff like "(a) = 1"
    } else if(t->type == TOK_SYMBOL) {
        *cidx = genScalarConstant(p, t);
        return OP_SETSYM;
    } else if(t->type == TOK_DOT && RIGHT(t) && RIGHT(t)->type == TOK_SYMBOL) {
        genExpr(p, LEFT(t));
        *cidx = genScalarConstant(p, RIGHT(t));
        return OP_SETMEMBER;
    } else if(t->type == TOK_LBRA) {
        genExpr(p, LEFT(t));
        genExpr(p, RIGHT(t));
        return OP_INSERT;
    } else if(t->type == TOK_VAR && RIGHT(t) && RIGHT(t)->type == TOK_SYMBOL) {
        *cidx = genScalarConstant(p, RIGHT(t));
        return OP_SETLOCAL;
    } else {
        naParseError(p, "bad lvalue", t->line);
        return -1;
    }
}

static void genEqOp(int op, struct Parser* p, struct Token* t)
{
    int cidx, n = 2, setop = genLValue(p, LEFT(t), &cidx);
    if(setop == OP_SETMEMBER) {
        emit(p, OP_DUP2);
        emit(p, OP_POP);
        emitImmediate(p, OP_MEMBER, cidx);
    } else if(setop == OP_INSERT) {
        emit(p, OP_DUP2);
        emit(p, OP_EXTRACT);
    } else {
        emitImmediate(p, OP_LOCAL, cidx);
        n = 1;
    }
    genExpr(p, RIGHT(t));
    emit(p, op);
    emit(p, n == 1 ? OP_XCHG : OP_XCHG2);
    emit(p, setop);
}

static int defArg(struct Parser* p, struct Token* t)
{
    if(t->type == TOK_LPAR) return defArg(p, RIGHT(t));
    if(t->type == TOK_MINUS && RIGHT(t) && 
       RIGHT(t)->type == TOK_LITERAL && !RIGHT(t)->str)
    {
        /* default arguments are constants, but "-1" parses as two
         * tokens, so we have to subset the expression generator for that
         * case */
        RIGHT(t)->num *= -1;
        return defArg(p, RIGHT(t));
    }
    return findConstantIndex(p, t);
}

static void genArgList(struct Parser* p, struct naCode* c, struct Token* t)
{
    naRef sym;
    if(t->type == TOK_EMPTY) return;
    if(!IDENTICAL(p->cg->restArgSym, globals->argRef))
        naParseError(p, "remainder must be last", t->line);
    if(t->type == TOK_ELLIPSIS) {
        if(LEFT(t)->type != TOK_SYMBOL)
            naParseError(p, "bad function argument expression", t->line);
        sym = naStr_fromdata(naNewString(p->context),
                             LEFT(t)->str, LEFT(t)->strlen);
        p->cg->restArgSym = naInternSymbol(sym);
        c->needArgVector = 1;
    } else if(t->type == TOK_ASSIGN) {
        if(LEFT(t)->type != TOK_SYMBOL)
            naParseError(p, "bad function argument expression", t->line);
        p->cg->optArgSyms[c->nOptArgs] = findConstantIndex(p, LEFT(t));
        p->cg->optArgVals[c->nOptArgs++] = defArg(p, RIGHT(t));
    } else if(t->type == TOK_SYMBOL) {
        if(c->nOptArgs)
            naParseError(p, "optional arguments must be last", t->line);
        if(c->nArgs >= MAX_FUNARGS)
            naParseError(p, "too many named function arguments", t->line);
        p->cg->argSyms[c->nArgs++] = findConstantIndex(p, t);
    } else if(t->type == TOK_COMMA) {
        if(!LEFT(t) || !RIGHT(t))
            naParseError(p, "empty function argument", t->line);
        genArgList(p, c, LEFT(t));
        genArgList(p, c, RIGHT(t));
    } else
        naParseError(p, "bad function argument expression", t->line);
}

static naRef newLambda(struct Parser* p, struct Token* t)
{
    struct CodeGenerator* cgSave;
    naRef codeObj;
    struct Token* arglist;
    if(RIGHT(t)->type != TOK_LCURL)
        naParseError(p, "bad function definition", t->line);

    // Save off the generator state while we do the new one
    cgSave = p->cg;
    arglist = LEFT(t)->type == TOK_LPAR ? LEFT(LEFT(t)) : 0;
    codeObj = naCodeGen(p, LEFT(RIGHT(t)), arglist);
    p->cg = cgSave;
    return codeObj;
}

static void genLambda(struct Parser* p, struct Token* t)
{
    emitImmediate(p, OP_PUSHCONST, newConstant(p, newLambda(p, t)));
}

static int genList(struct Parser* p, struct Token* t, int doAppend)
{
    if(!t || t->type == TOK_EMPTY) {
        return 0;
    } else if(t->type == TOK_COMMA) {
        genExpr(p, LEFT(t));
        if(doAppend) emit(p, OP_VAPPEND);
        return 1 + genList(p, RIGHT(t), doAppend);
    } else {
        genExpr(p, t);
        if(doAppend) emit(p, OP_VAPPEND);
        return 1;
    }
}

static void genHashElem(struct Parser* p, struct Token* t)
{
    if(!t || t->type == TOK_EMPTY)
        return;
    if(t->type != TOK_COLON || !LEFT(t))
        naParseError(p, "bad hash/object initializer", t->line);
    if(LEFT(t)->type == TOK_SYMBOL) genScalarConstant(p, LEFT(t));
    else if(LEFT(t)->type == TOK_LITERAL) genExpr(p, LEFT(t));
    else naParseError(p, "bad hash/object initializer", t->line);
    genExpr(p, RIGHT(t));
    emit(p, OP_HAPPEND);
}

static void genHash(struct Parser* p, struct Token* t)
{
    if(t && t->type == TOK_COMMA) {
        genHashElem(p, LEFT(t));
        genHash(p, RIGHT(t));
    } else if(t && t->type != TOK_EMPTY) {
        genHashElem(p, t);
    }
}

static int isHashcall(struct Parser* p, struct Token* t)
{
    if(t) {
        int sep = LEFT(t) && t->type == TOK_COMMA ? t->children->type : t->type;
        return sep == TOK_COLON;
    }
    return 0;
}

static void genFuncall(struct Parser* p, struct Token* t)
{
    int method = 0;
    if(LEFT(t)->type == TOK_DOT) {
        method = 1;
        genExpr(p, LEFT(LEFT(t)));
        emit(p, OP_DUP);
        emitImmediate(p, OP_MEMBER, findConstantIndex(p, RIGHT(LEFT(t))));
    } else {
        genExpr(p, LEFT(t));
    }
    if(isHashcall(p, RIGHT(t))) {
        emit(p, OP_NEWHASH);
        genHash(p, RIGHT(t));
        emit(p, method ? OP_MCALLH : OP_FCALLH);
    } else {
        int nargs = genList(p, RIGHT(t), 0);
        emitImmediate(p, method ? OP_MCALL : OP_FCALL, nargs);
    }
}

static int startLoop(struct Parser* p, struct Token* label)
{
    int i = p->cg->loopTop;
    p->cg->loops[i].breakIP = 0xffffff;
    p->cg->loops[i].contIP = 0xffffff;
    p->cg->loops[i].label = label;
    p->cg->loopTop++;
    emit(p, OP_MARK);
    return p->cg->codesz;
}

// Emit a jump operation, and return the location of the address in
// the bytecode for future fixup in fixJumpTarget
static int emitJump(struct Parser* p, int op)
{
    int ip;
    emit(p, op);
    ip = p->cg->codesz;
    emit(p, 0xffff); // dummy address
    return ip;
}

// Points a previous jump instruction at the current "end-of-bytecode"
static void fixJumpTarget(struct Parser* p, int spot)
{
    p->cg->byteCode[spot] = p->cg->codesz;
}

static void genShortCircuit(struct Parser* p, struct Token* t)
{
    int end;
    genExpr(p, LEFT(t));
    end = emitJump(p, t->type == TOK_AND ? OP_JIFNOT : OP_JIFTRUE);
    emit(p, OP_POP);
    genExpr(p, RIGHT(t));
    fixJumpTarget(p, end);
}


static void genIf(struct Parser* p, struct Token* tif, struct Token* telse)
{
    int jumpNext, jumpEnd;
    genExpr(p, tif->children); // the test
    jumpNext = emitJump(p, OP_JIFNOTPOP);
    genExprList(p, tif->children->next->children); // the body
    jumpEnd = emitJump(p, OP_JMP);
    fixJumpTarget(p, jumpNext);
    if(telse) {
        if(telse->type == TOK_ELSIF) genIf(p, telse, telse->next);
        else genExprList(p, telse->children->children);
    } else {
        emit(p, OP_PUSHNIL);
    }
    fixJumpTarget(p, jumpEnd);
}

static void genIfElse(struct Parser* p, struct Token* t)
{
    genIf(p, t, t->children->next->next);
}

static void genQuestion(struct Parser* p, struct Token* t)
{
    int jumpNext, jumpEnd;
    if(!RIGHT(t) || RIGHT(t)->type != TOK_COLON)
        naParseError(p, "invalid ?: expression", t->line);
    genExpr(p, LEFT(t)); // the test
    jumpNext = emitJump(p, OP_JIFNOTPOP);
    genExpr(p, LEFT(RIGHT(t))); // the "if true" expr
    jumpEnd = emitJump(p, OP_JMP);
    fixJumpTarget(p, jumpNext);
    genExpr(p, RIGHT(RIGHT(t))); // the "else" expr
    fixJumpTarget(p, jumpEnd);
}

static int countList(struct Token* t, int type)
{
    int n;
    for(n = 1; t && t->type == type; t = RIGHT(t)) n++;
    return n;
}

static void genLoop(struct Parser* p, struct Token* body,
                    struct Token* update, struct Token* label,
                    int loopTop, int jumpEnd)
{
    int cont, jumpOverContinue;
    
    p->cg->loops[p->cg->loopTop-1].breakIP = jumpEnd-1;

    jumpOverContinue = emitJump(p, OP_JMP);
    p->cg->loops[p->cg->loopTop-1].contIP = p->cg->codesz;
    cont = emitJump(p, OP_JMP);
    fixJumpTarget(p, jumpOverContinue);

    genExprList(p, body);
    emit(p, OP_POP);
    fixJumpTarget(p, cont);
    if(update) { genExpr(p, update); emit(p, OP_POP); }
    emitImmediate(p, OP_JMPLOOP, loopTop);
    fixJumpTarget(p, jumpEnd);
    p->cg->loopTop--;
    emit(p, OP_UNMARK);
    emit(p, OP_PUSHNIL); // Leave something on the stack
}

static void genForWhile(struct Parser* p, struct Token* init,
                        struct Token* test, struct Token* update,
                        struct Token* body, struct Token* label)
{
    int loopTop, jumpEnd;
    if(init) { genExpr(p, init); emit(p, OP_POP); }
    loopTop = startLoop(p, label);
    genExpr(p, test);
    jumpEnd = emitJump(p, OP_JIFNOTPOP);
    genLoop(p, body, update, label, loopTop, jumpEnd);
}

static void genWhile(struct Parser* p, struct Token* t)
{
    struct Token *test=LEFT(t)->children, *body, *label=0;
    int len = countList(test, TOK_SEMI);
    if(len == 2) {
        label = LEFT(test);
        if(!label || label->type != TOK_SYMBOL)
            naParseError(p, "bad loop label", t->line);
        test = RIGHT(test);
    } else if(len != 1)
        naParseError(p, "too many semicolons in while test", t->line);
    body = LEFT(RIGHT(t));
    genForWhile(p, 0, test, 0, body, label);
}

static void genFor(struct Parser* p, struct Token* t)
{
    struct Token *init, *test, *body, *update, *label=0;
    struct Token *h = LEFT(t)->children;
    int len = countList(h, TOK_SEMI);
    if(len == 4) {
        if(!LEFT(h) || LEFT(h)->type != TOK_SYMBOL)
            naParseError(p, "bad loop label", h->line);
        label = LEFT(h);
        h=RIGHT(h);
    } else if(len != 3)
        naParseError(p, "wrong number of terms in for header", t->line);
    init = LEFT(h);
    test = LEFT(RIGHT(h));
    update = RIGHT(RIGHT(h));
    body = RIGHT(t)->children;
    genForWhile(p, init, test, update, body, label);
}

static void genForEach(struct Parser* p, struct Token* t)
{
    int loopTop, jumpEnd, assignOp, dummy;
    struct Token *elem, *body, *vec, *label=0;
    struct Token *h = LEFT(LEFT(t));
    int len = countList(h, TOK_SEMI);
    if(len == 3) {
        if(!LEFT(h) || LEFT(h)->type != TOK_SYMBOL)
            naParseError(p, "bad loop label", h->line);
        label = LEFT(h);
        h = RIGHT(h);
    } else if (len != 2) {
        naParseError(p, "wrong number of terms in foreach header", t->line);
    }
    elem = LEFT(h);
    vec = RIGHT(h);
    body = RIGHT(t)->children;

    genExpr(p, vec);
    emit(p, OP_PUSHZERO);
    loopTop = startLoop(p, label);
    emit(p, t->type == TOK_FOREACH ? OP_EACH : OP_INDEX);
    jumpEnd = emitJump(p, OP_JIFEND);
    assignOp = genLValue(p, elem, &dummy);
    emit(p, assignOp);
    emit(p, OP_POP);
    genLoop(p, body, 0, label, loopTop, jumpEnd);
    emit(p, OP_POP); // Pull off the vector and index
    emit(p, OP_POP);
}

static int tokMatch(struct Token* a, struct Token* b)
{
    int i, l = a->strlen;
    if(!a || !b) return 0;
    if(l != b->strlen) return 0;
    for(i=0; i<l; i++) if(a->str[i] != b->str[i]) return 0;
    return 1;
}

static void genBreakContinue(struct Parser* p, struct Token* t)
{
    int levels = 1, loop = -1, bp, cp, i;
    if(RIGHT(t)) {
        if(RIGHT(t)->type != TOK_SYMBOL)
            naParseError(p, "bad break/continue label", t->line);
        for(i=0; i<p->cg->loopTop; i++)
            if(tokMatch(RIGHT(t), p->cg->loops[i].label))
                loop = i;
        if(loop == -1)
            naParseError(p, "no match for break/continue label", t->line);
        levels = p->cg->loopTop - loop;
    }
    bp = p->cg->loops[p->cg->loopTop - levels].breakIP;
    cp = p->cg->loops[p->cg->loopTop - levels].contIP;
    for(i=0; i<levels; i++)
        emit(p, (i<levels-1) ? OP_BREAK2 : OP_BREAK);
    if(t->type == TOK_BREAK)
        emit(p, OP_PUSHEND); // breakIP is always a JIFNOTPOP/JIFEND!
    emitImmediate(p, OP_JMP, t->type == TOK_BREAK ? bp : cp);
}

static void newLineEntry(struct Parser* p, int line)
{
    int i;
    if(p->cg->nextLineIp >= p->cg->nLineIps) {
        int nsz = p->cg->nLineIps*2 + 1;
        unsigned short* n = naParseAlloc(p, sizeof(unsigned short)*2*nsz);
        for(i=0; i<(p->cg->nextLineIp*2); i++)
            n[i] = p->cg->lineIps[i];
        p->cg->lineIps = n;
        p->cg->nLineIps = nsz;
    }
    p->cg->lineIps[p->cg->nextLineIp++] = (unsigned short) p->cg->codesz;
    p->cg->lineIps[p->cg->nextLineIp++] = (unsigned short) line;
}

static int parListLen(struct Token* t)
{
    if(t->type != TOK_LPAR || !LEFT(t) || LEFT(t)->type != TOK_COMMA) return 0;
    return countList(LEFT(t), TOK_COMMA);
}

static void genCommaList(struct Parser* p, struct Token* t)
{
    if(t->type != TOK_COMMA) { genExpr(p, t); return; }
    genCommaList(p, RIGHT(t));
    genExpr(p, LEFT(t));
}

static void genMultiLV(struct Parser* p, struct Token* t, int var)
{
    if(!var) { emit(p, genLValue(p, t, &var)); return; }
    if(t->type != TOK_SYMBOL) naParseError(p, "bad lvalue", t->line);
    genScalarConstant(p, t);
    emit(p, OP_SETLOCAL);
}

static void genAssign(struct Parser* p, struct Token* t)
{
    struct Token *lv = LEFT(t), *rv = RIGHT(t);
    int len, dummy, var=0;
    if(parListLen(lv) || (lv->type == TOK_VAR && parListLen(RIGHT(lv)))) {
        if(lv->type == TOK_VAR) { lv = RIGHT(lv); var = 1; }
        len = parListLen(lv);
        if(rv->type == TOK_LPAR) {
            if(len != parListLen(rv))
                naParseError(p, "bad assignment count", rv->line);
            genCommaList(p, LEFT(rv));
        } else {
            genExpr(p, rv);
            emitImmediate(p, OP_UNPACK, len);
        }
        for(t = LEFT(lv); t && t->type == TOK_COMMA; t = RIGHT(t)) {
            genMultiLV(p, LEFT(t), var);
            emit(p, OP_POP);
        }
        genMultiLV(p, t, var);
    } else {
        genExpr(p, rv);
        emit(p, genLValue(p, lv, &dummy));
    }
}

static void genSlice(struct Parser* p, struct Token* t)
{
    if(!t || t->type==TOK_EMPTY) naParseError(p, "empty slice expression", -1);
    if(t->type == TOK_COLON) {
        if(LEFT(t))  genExpr(p, LEFT(t));  else emit(p, OP_PUSHNIL);
        if(RIGHT(t)) genExpr(p, RIGHT(t)); else emit(p, OP_PUSHNIL);
        emit(p, OP_SLICE2);
    } else {
        genExpr(p, t);
        emit(p, OP_SLICE);
    }
}

static void genExtract(struct Parser* p, struct Token* t)
{
    genExpr(p, LEFT(t));
    if(countList(RIGHT(t), TOK_COMMA) == 1 && RIGHT(t)->type != TOK_COLON) {
        genExpr(p, RIGHT(t));
        emit(p, OP_EXTRACT);
    } else {
        emit(p, OP_NEWVEC);
        for(t = RIGHT(t); t->type == TOK_COMMA; t = RIGHT(t))
            genSlice(p, LEFT(t));
        genSlice(p, t);
        emit(p, OP_XCHG);
        emit(p, OP_POP);
    }
}

static void genExpr(struct Parser* p, struct Token* t)
{
    int i;
    if(!t) naParseError(p, "parse error", -1); // throw line -1...
    p->errLine = t->line;                      // ...to use this one instead
    if(t->line != p->cg->lastLine)
        newLineEntry(p, t->line);
    p->cg->lastLine = t->line;
    switch(t->type) {
    case TOK_TOP:      genExprList(p, LEFT(t)); break;
    case TOK_IF:       genIfElse(p, t);   break;
    case TOK_QUESTION: genQuestion(p, t); break;
    case TOK_WHILE:    genWhile(p, t);    break;
    case TOK_FOR:      genFor(p, t);      break;
    case TOK_FUNC:     genLambda(p, t);   break;
    case TOK_ASSIGN:   genAssign(p, t);   break;
    case TOK_LITERAL:  genScalarConstant(p, t); break;
    case TOK_FOREACH: case TOK_FORINDEX:
        genForEach(p, t);
        break;
    case TOK_BREAK: case TOK_CONTINUE:
        genBreakContinue(p, t);
        break;
    case TOK_LPAR:
        if(BINARY(t) || !RIGHT(t)) genFuncall(p, t);
        else genExpr(p, LEFT(t));
        break;
    case TOK_LBRA:
        if(BINARY(t)) {
            genExtract(p, t);
        } else {
            emit(p, OP_NEWVEC);
            genList(p, LEFT(t), 1);
        }
        break;
    case TOK_LCURL:
        emit(p, OP_NEWHASH);
        genHash(p, LEFT(t));
        break;
    case TOK_RETURN:
        if(RIGHT(t)) genExpr(p, RIGHT(t));
        else emit(p, OP_PUSHNIL);
        for(i=0; i<p->cg->loopTop; i++) emit(p, OP_UNMARK);
        emit(p, OP_RETURN);
        break;
    case TOK_NOT:
        genExpr(p, RIGHT(t));
        emit(p, OP_NOT);
        break;
    case TOK_SYMBOL:
        emitImmediate(p, OP_LOCAL, findConstantIndex(p, t));
        break;
    case TOK_MINUS:
        if(BINARY(t)) {
            genBinOp(OP_MINUS,  p, t);  // binary subtraction
        } else if(RIGHT(t) && RIGHT(t)->type == TOK_LITERAL && !RIGHT(t)->str) {
            RIGHT(t)->num *= -1;        // Pre-negate constants
            genScalarConstant(p, RIGHT(t));
        } else {
            genExpr(p, RIGHT(t));       // unary negation
            emit(p, OP_NEG);
        }
        break;
    case TOK_NEG:
        genExpr(p, RIGHT(t)); // unary negation (see also TOK_MINUS!)
        emit(p, OP_NEG);
        break;
    case TOK_DOT:
        genExpr(p, LEFT(t));
        if(!RIGHT(t) || RIGHT(t)->type != TOK_SYMBOL)
            naParseError(p, "object field not symbol", RIGHT(t)->line);
        emitImmediate(p, OP_MEMBER, findConstantIndex(p, RIGHT(t)));
        break;
    case TOK_EMPTY: case TOK_NIL:
        emit(p, OP_PUSHNIL);
        break;
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
    case TOK_PLUSEQ:  genEqOp(OP_PLUS, p, t);  break;
    case TOK_MINUSEQ: genEqOp(OP_MINUS, p, t); break;
    case TOK_MULEQ:   genEqOp(OP_MUL, p, t);   break;
    case TOK_DIVEQ:   genEqOp(OP_DIV, p, t);   break;
    case TOK_CATEQ:   genEqOp(OP_CAT, p, t);   break;
    default:
        naParseError(p, "parse error", t->line);
    };
}

static void genExprList(struct Parser* p, struct Token* t)
{
    if(t && t->type == TOK_SEMI) {
        genExpr(p, LEFT(t));
        if(RIGHT(t) && RIGHT(t)->type != TOK_EMPTY) {
            emit(p, OP_POP);
            genExprList(p, RIGHT(t));
        }
    } else {
        genExpr(p, t);
    }
}

naRef naCodeGen(struct Parser* p, struct Token* block, struct Token* arglist)
{
    int i;
    naRef codeObj;
    struct naCode* code;
    struct CodeGenerator cg;

    cg.lastLine = 0;
    cg.codeAlloced = 1024; // Start fairly big, this is a cheap allocation
    cg.byteCode = naParseAlloc(p, cg.codeAlloced *sizeof(unsigned short));
    cg.codesz = 0;
    cg.consts = naNewVector(p->context);
    cg.loopTop = 0;
    cg.lineIps = 0;
    cg.nLineIps = 0;
    cg.nextLineIp = 0;
    p->cg = &cg;

    genExprList(p, block);
    emit(p, OP_RETURN);
    
    // Now make a code object
    codeObj = naNewCode(p->context);
    code = PTR(codeObj).code;
    
    // Parse the argument list, if any
    p->cg->restArgSym = globals->argRef;
    code->nArgs = code->nOptArgs = 0;
    p->cg->argSyms = p->cg->optArgSyms = p->cg->optArgVals = 0;
    code->needArgVector = 1;
    if(arglist) {
        p->cg->argSyms    = naParseAlloc(p, sizeof(int) * MAX_FUNARGS);
        p->cg->optArgSyms = naParseAlloc(p, sizeof(int) * MAX_FUNARGS);
        p->cg->optArgVals = naParseAlloc(p, sizeof(int) * MAX_FUNARGS);
        code->needArgVector = 0;
        genArgList(p, code, arglist);
    }

    code->restArgSym = internConstant(p, p->cg->restArgSym);

    /* Set the size fields and allocate the combined array buffer.
     * Note cute trick with null pointer to get the array size. */
    code->nConstants = naVec_size(cg.consts);
    code->codesz = cg.codesz;
    code->nLines = cg.nextLineIp;
    code->srcFile = p->srcFile;
    code->constants = 0;
    code->constants = naAlloc((int)(size_t)(LINEIPS(code)+code->nLines));
    for(i=0; i<code->nConstants; i++)
        code->constants[i] = naVec_get(p->cg->consts, i);

    for(i=0; i<code->nArgs; i++) ARGSYMS(code)[i] = cg.argSyms[i];
    for(i=0; i<code->nOptArgs; i++) OPTARGSYMS(code)[i] = cg.optArgSyms[i];
    for(i=0; i<code->nOptArgs; i++) OPTARGVALS(code)[i] = cg.optArgVals[i];
    for(i=0; i<code->codesz; i++) BYTECODE(code)[i] = cg.byteCode[i];
    for(i=0; i<code->nLines; i++) LINEIPS(code)[i] = cg.lineIps[i];

    return codeObj;
}
