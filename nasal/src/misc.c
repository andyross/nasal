#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "nasl.h"
#include "code.h"

void FREE(void* m) { free(m); }
void* ALLOC(int n) { return malloc(n); }
void ERR(char* msg) { fprintf(stderr, "%s\n", msg); *(int*)0=0; }
void BZERO(void* m, int n) { bzero(m, n); }

int naTrue(naRef r)
{
    if(IS_NIL(r)) return 0;
    if(IS_NUM(r)) return r.num != 0;
    if(IS_STR(r)) return 1;
    ERR("non-scalar used in boolean context");
    return 0;
}

naRef naContainerGet(naRef box, naRef key)
{
    if(IS_HASH(box)) return naHash_get(box, key);
    if(IS_VEC(box) && IS_NUM(key))
        return naVec_get(box, (int)key.num);
    return naNil();
}

naRef naNew(struct Context* c, int type)
{
    struct naObj* o;
    if((o = naGC_get(&(c->pools[type]))) == 0)
        naGarbageCollect();
    if((o = naGC_get(&(c->pools[type]))) == 0)
        ERR("Out of memory");
    return naObj(type, o);
}

naRef naNewString(struct Context* c)
{
    return naNew(c, T_STR);
}

naRef naNewVector(struct Context* c)
{
    naRef r = naNew(c, T_VEC);
    naVec_init(r);
    return r;
}

naRef naNewHash(struct Context* c)
{
    naRef r = naNew(c, T_HASH);
    naHash_init(r);
    return r;
}

naRef naNewCode(struct Context* c)
{
    return naNew(c, T_CODE);
}

naRef naNewClosure(struct Context* c)
{
    return naNew(c, T_CLOSURE);
}

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
    else if(!(IS_STR(a) && naStr_tonum(a, &na))) { return 0; }

    if(IS_NUM(b)) { nb = b.num; }
    else if(!(IS_STR(b) && naStr_tonum(b, &nb))) { return 0; }

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
