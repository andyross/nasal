#ifndef _CODE_H
#define _CODE_H

#include "nasl.h"

#define MAX_STACK_DEPTH 1024
#define MAX_RECURSION 128

enum {    
    OP_AND, OP_OR, OP_NOT, OP_MUL, OP_PLUS, OP_MINUS, OP_DIV, OP_NEG,
    OP_CAT, OP_LT, OP_LTE, OP_GT, OP_GTE, OP_EQ, OP_NEQ, OP_EACH,
    OP_JMP, OP_JIFNOT, OP_JIFNIL, OP_FCALL, OP_MCALL, OP_RETURN,
    OP_PUSHCONST, OP_PUSHONE, OP_PUSHZERO, OP_PUSHNIL, OP_POP,
    OP_DUP, OP_XCHG, OP_INSERT, OP_EXTRACT, OP_MEMBER, OP_SETMEMBER,
    OP_LOCAL, OP_SETLOCAL, OP_NEWVEC, OP_VAPPEND, OP_NEWHASH, OP_HAPPEND,
    OP_LINE
};

struct Frame {
    naRef code; // naCode object
    naRef namespace; // lexical closure of the function object
    naRef locals; // local per-call namespace
    int ip; // instruction pointer into code
    int bp; // opStack pointer to start of frame
    int line; // current line number
};

struct Context {
    struct naPool pools[NUM_NASL_TYPES];
    struct Frame fStack[MAX_RECURSION];
    int fTop;
    naRef opStack[MAX_STACK_DEPTH];
    int opTop;
    int done;
    naRef meRef;
    naRef argRef;
};

void naRun(struct Context* ctx, naRef code);

void printRefDEBUG(naRef r);

#endif // _CODE_H
