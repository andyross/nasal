#ifndef _CODE_H
#define _CODE_H

#include "nasl.h"

enum {
    OP_AND, OP_OR, OP_NOT, OP_MUL, OP_PLUS, OP_MINUS, OP_DIV,
    OP_CAT, OP_LT, OP_LTE, OP_EQ, OP_NEQ, OP_GT, OP_GTE,
    OP_RETURN, OP_ASSIGN
};

struct Context
{
    struct naPool pools[NUM_NASL_TYPES];
    naRef opStack; // A naVec object
};

#endif // _CODE_H
