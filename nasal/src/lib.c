#include "nasl.h"

// No need to include <string.h> just for this
static int strlen(char* s)
{
    char* s0 = s;
    while(*s) s++;
    return s - s0;
}

static naRef size(naContext c, naRef args)
{
    naRef r;
    if(naVec_size(args) == 0) return naNil();
    r = naVec_get(args, 0);
    if(IS_STR(r)) return naNum(r.ref.ptr.str->len);
    if(IS_VEC(r)) return naNum(r.ref.ptr.vec->size);
    if(IS_HASH(r)) return naNum(r.ref.ptr.vec->size);
    return naNil();
}

static naRef keys(naContext c, naRef args)
{
    naRef v, h = naVec_get(args, 0);
    if(!IS_HASH(h)) return naNil();
    v = naNewVector(c);
    naHash_keys(v, h);
    return v;
}

static naRef append(naContext c, naRef args)
{
    naRef v = naVec_get(args, 0);
    naRef e = naVec_get(args, 1);
    if(!IS_VEC(v)) return naNil();
    naVec_append(v, e);
    return v;
}

static naRef pop(naContext c, naRef args)
{
    naRef v = naVec_get(args, 0);
    if(!IS_VEC(v)) return naNil();
    return naVec_removelast(v);
}

static naRef intf(naContext c, naRef args)
{
    naRef n = naNumValue(naVec_get(args, 0));
    if(!IS_NIL(n)) n.num = (int)n.num;
    return n;
}

static naRef streq(naContext c, naRef args)
{
    int i;
    naRef a = naVec_get(args, 0);
    naRef b = naVec_get(args, 1);
    if(!IS_STR(a) || !IS_STR(b)) return naNil();
    if(naStr_len(a) != naStr_len(b)) return naNum(0);
    for(i=0; i<naStr_len(a); i++)
        if(naStr_data(a)[i] != naStr_data(b)[i])
            return naNum(0);
    return naNum(1);
}

static naRef substr(naContext c, naRef args)
{
    naRef s;
    naRef src = naVec_get(args, 0);
    naRef startR = naVec_get(args, 1);
    naRef lenR = naVec_get(args, 2);
    int start, len;
    if(!IS_STR(src)) return naNil();
    startR = naNumValue(startR);
    if(IS_NIL(startR)) return naNil();
    start = (int)startR.num;
    if(IS_NIL(lenR)) {
        len = naStr_len(src) - start;
    } else {
        lenR = naNumValue(lenR);
        if(IS_NIL(lenR)) return naNil();
        len = (int)lenR.num;
    }
    s = naNewString(c);
    naStr_substr(s, src, start, len);
    return s;
}

static naRef contains(naContext c, naRef args)
{
    naRef hash = naVec_get(args, 0);
    naRef key = naVec_get(args, 1);
    if(IS_NIL(hash) || IS_NIL(key)) return naNil();
    if(!IS_HASH(hash)) return naNil();
    return naHash_get(hash, key, &key) ? naNum(1) : naNum(0);
}

static naRef typeOf(naContext c, naRef args)
{
    naRef r = naVec_get(args, 0);
    char* t = "unknown";
    if(IS_NIL(r)) t = "nil";
    else if(IS_NUM(r)) t = "scalar";
    else if(IS_STR(r)) t = "scalar";
    else if(IS_VEC(r)) t = "vector";
    else if(IS_HASH(r)) t = "hash";
    else if(IS_FUNC(r)) t = "func";
    r = naStr_fromdata(naNewString(c), t, strlen(t));
    return r;
}

struct func { char* name; naCFunction func; };
struct func funcs[] = {
    { "size", size },
    { "keys", keys }, 
    { "append", append }, 
    { "pop", pop }, 
    { "int", intf },
    { "streq", streq },
    { "substr", substr },
    { "contains", contains },
    { "typeof", typeOf },
};

naRef naStdLib(naContext c)
{
    naRef namespace = naNewHash(c);
    int i, n = sizeof(funcs)/sizeof(struct func);
    for(i=0; i<n; i++) {
        naRef code = naNewCCode(c, funcs[i].func);
        naRef name = naStr_fromdata(naNewString(c),
                                    funcs[i].name, strlen(funcs[i].name));
        naHash_set(namespace, name, naNewFunc(c, code, naNil()));
    }
    return namespace;
}
