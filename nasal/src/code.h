#ifndef _CODE_H
#define _CODE_H

#include "nasl.h"

#define MAX_STACK_DEPTH 1024
#define MAX_RECURSION 128

enum {    
    OP_AND, OP_OR, OP_NOT, OP_MUL, OP_PLUS, OP_MINUS, OP_DIV,
    OP_CAT, OP_LT, OP_LTE, OP_EQ, OP_NEQ, OP_GT, OP_GTE,
    OP_RETURN, OP_ASSIGN, OP_DUP, OP_PUSHCONST, OP_PUSHNIL,
    OP_INSERT, OP_EXTRACT, OP_MEMBER, OP_SETMEMBER, OP_POP,
    OP_LOCAL, OP_SETLOCAL, OP_NEG
};

struct Frame {
    naRef code; // naCode object
    naRef namespace; // lexical closure of the function object
    naRef locals; // local per-call namespace
    int ip; // instruction pointer into code
    int bp; // opStack pointer to start of frame
};

struct Context {
    struct naPool pools[NUM_NASL_TYPES];

    struct Frame fStack[MAX_RECURSION];
    int fTop;

    naRef opStack[MAX_STACK_DEPTH];
    int opTop;

    int done;
};

void naRun(struct Context* ctx, naRef code);

#endif // _CODE_H
