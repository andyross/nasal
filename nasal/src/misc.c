#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "nasl.h"

void FREE(void* m) { free(m); }
void* ALLOC(int n) { return malloc(n); }
void ERR(char* msg) { fprintf(stderr, "%s\n", msg); *(int*)0=0; }
void BZERO(void* m, int n) { bzero(m, n); }

naRef naNil()
{
    naRef r;
    r.ref.reftag = NASL_REFTAG;
    r.ref.ptr.obj = 0;
    return r;
}

naRef naNum(double num)
{
    naRef r;
    r.num = num;
    return r;
}

naRef naObj(int type, struct naObj* o)
{
    naRef r;
    r.ref.reftag = NASL_REFTAG;
    r.ref.ptr.obj = o;
    o->type = type;
    return r;
}

int naEqual(naRef a, naRef b)
{
    double na=0, nb=0;
    if(IS_REF(a) && IS_REF(b) && a.ref.ptr.obj == b.ref.ptr.obj)
        return 1; // Object identity
    if(IS_NUM(a) && IS_NUM(b) && a.num == b.num)
        return 1; // Numeric equality
    if(IS_STR(a) && IS_STR(b) && naStr_equal(a, b))
        return 1; // String equality

    // Numeric equality after conversion
    if(IS_NUM(a)) { na = a.num; }
    else if(!(IS_STR(a) && naStr_numeric(a))) { return 0; }

    if(IS_NUM(b)) { nb = b.num; }
    else if(!(IS_STR(b) && naStr_numeric(b))) { return 0; }

    return na == nb;
}

int naTypeSize(int type)
{
    switch(type) {
    case T_STR: return sizeof(struct naStr);
    case T_VEC: return sizeof(struct naVec);
    case T_HASH: return sizeof(struct naHash);
    case T_CODE: return sizeof(struct naCode);
    case T_CLOSURE: return sizeof(struct naClosure);
    };
    return -1;
}
