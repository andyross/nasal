#ifndef _CODE_H
#define _CODE_H

#include "nasl.h"

enum {
    OP_AND, OP_OR, OP_NOT, OP_MUL, OP_PLUS, OP_MINUS, OP_DIV,
    OP_CAT, OP_LT, OP_LTE, OP_EQ, OP_NEQ, OP_GT, OP_GTE,
    OP_RETURN, OP_ASSIGN
};

struct Frame {
    naRef func;
    naRef namespace;
    naRef* opStack;
    int opTop;
    int opDepth;
};

struct Context {
    struct naPool pools[NUM_NASL_TYPES];
    struct Frame* stack;
    int top;
    int depth;
};

#endif // _CODE_H
