#include <math.h>
#include <string.h>

#include "nasal.h"
#include "code.h"

static naRef size(naContext c, naRef me, naRef args)
{
    naRef r;
    if(naVec_size(args) == 0) return naNil();
    r = naVec_get(args, 0);
    if(naIsString(r)) return naNum(naStr_len(r));
    if(naIsVector(r)) return naNum(naVec_size(r));
    if(naIsHash(r)) return naNum(naHash_size(r));
    return naNil();
}

static naRef keys(naContext c, naRef me, naRef args)
{
    naRef v, h = naVec_get(args, 0);
    if(!naIsHash(h)) return naNil();
    v = naNewVector(c);
    naHash_keys(v, h);
    return v;
}

static naRef append(naContext c, naRef me, naRef args)
{
    naRef v = naVec_get(args, 0);
    naRef e = naVec_get(args, 1);
    if(!naIsVector(v)) return naNil();
    naVec_append(v, e);
    return v;
}

static naRef pop(naContext c, naRef me, naRef args)
{
    naRef v = naVec_get(args, 0);
    if(!naIsVector(v)) return naNil();
    return naVec_removelast(v);
}

static naRef setsize(naContext c, naRef me, naRef args)
{
    naRef v = naVec_get(args, 0);
    int sz = (int)naNumValue(naVec_get(args, 1)).num;
    if(!naIsVector(v)) return naNil();
    naVec_setsize(v, sz);
    return v;
}

static naRef subvec(naContext c, naRef me, naRef args)
{
    int i;
    naRef nlen, result, v = naVec_get(args, 0);
    int len = 0, start = (int)naNumValue(naVec_get(args, 1)).num;
    nlen = naNumValue(naVec_get(args, 2));
    if(!naIsNil(nlen))
        len = (int)naNumValue(naVec_get(args, 2)).num;
    if(!naIsVector(v) || start < 0 || start >= naVec_size(v) || len < 0)
        return naNil();
    if(len == 0 || len > naVec_size(v) - start) len = naVec_size(v) - start;
    result = naNewVector(c);
    naVec_setsize(result, len);
    for(i=0; i<len; i++)
        naVec_set(result, i, naVec_get(v, start + i));
    return result;
}

static naRef delete(naContext c, naRef me, naRef args)
{
    naRef h = naVec_get(args, 0);
    naRef k = naVec_get(args, 1);
    if(naIsHash(h)) naHash_delete(h, k);
    return naNil();
}

static naRef intf(naContext c, naRef me, naRef args)
{
    naRef n = naNumValue(naVec_get(args, 0));
    if(naIsNil(n)) return n;
    if(n.num < 0) n.num = -floor(-n.num);
    else n.num = floor(n.num);
    return n;
}

static naRef num(naContext c, naRef me, naRef args)
{
    return naNumValue(naVec_get(args, 0));
}

static naRef streq(naContext c, naRef me, naRef args)
{
    naRef a = naVec_get(args, 0);
    naRef b = naVec_get(args, 1);
    return naNum(naStrEqual(a, b));
}

static naRef substr(naContext c, naRef me, naRef args)
{
    naRef src = naVec_get(args, 0);
    naRef startR = naVec_get(args, 1);
    naRef lenR = naVec_get(args, 2);
    int start, len;
    if(!naIsString(src)) return naNil();
    startR = naNumValue(startR);
    if(naIsNil(startR)) return naNil();
    start = (int)startR.num;
    if(naIsNil(lenR)) {
        len = naStr_len(src) - start;
    } else {
        lenR = naNumValue(lenR);
        if(naIsNil(lenR)) return naNil();
        len = (int)lenR.num;
    }
    return naStr_substr(naNewString(c), src, start, len);
}

static naRef contains(naContext c, naRef me, naRef args)
{
    naRef hash = naVec_get(args, 0);
    naRef key = naVec_get(args, 1);
    if(naIsNil(hash) || naIsNil(key)) return naNil();
    if(!naIsHash(hash)) return naNil();
    return naHash_get(hash, key, &key) ? naNum(1) : naNum(0);
}

static naRef typeOf(naContext c, naRef me, naRef args)
{
    naRef r = naVec_get(args, 0);
    char* t = "unknown";
    if(naIsNil(r)) t = "nil";
    else if(naIsNum(r)) t = "scalar";
    else if(naIsString(r)) t = "scalar";
    else if(naIsVector(r)) t = "vector";
    else if(naIsHash(r)) t = "hash";
    else if(naIsFunc(r)) t = "func";
    else if(naIsGhost(r)) t = "ghost";
    r = naStr_fromdata(naNewString(c), t, strlen(t));
    return r;
}

static naRef f_compile(naContext c, naRef me, naRef args)
{
    int errLine;
    naRef script, code, fname;
    script = naVec_get(args, 0);
    if(!naIsString(script)) return naNil();
    fname = naStr_fromdata(naNewString(c), "<compile>", 4);
    code = naParseCode(c, fname, 1,
                       naStr_data(script), naStr_len(script), &errLine);
    if(!naIsCode(code)) return naNil(); // FIXME: export error to caller...
    return naBindToContext(c, code);
}

// Funcation metacall API.  Allows user code to generate an arg vector
// at runtime and/or call function references on arbitrary objects.
static naRef f_call(naContext c, naRef me, naRef args)
{
    naContext subc;
    naRef func, callargs, callme, result;
    func = naVec_get(args, 0);
    callargs = naVec_get(args, 1);
    callme = naVec_get(args, 2); // Might be nil, that's OK
    if(!naIsFunc(func)) return naNil();
    if(naIsNil(callargs)) callargs = naNewVector(c);
    else if(!naIsVector(callargs)) return naNil();
    if(!naIsHash(callme)) callme = naNil();
    subc = naNewContext();
    result = naCall(subc, func, callargs, callme, naNil());
    if(naVec_size(args) > 2 &&
       naGetError(subc) && (strcmp(naGetError(subc), "__die__") == 0))
    {
        naRef ex = naVec_get(args, naVec_size(args) - 1);
        if(naIsVector(ex)) {
            naVec_append(ex, subc->dieArg);
            // FIXME: append stack trace
        }
    }
    naFreeContext(subc);
    return result;
}

static naRef f_die(naContext c, naRef me, naRef args)
{
    c->dieArg = naVec_get(args, 0);
    naRuntimeError(c, "__die__");
    return naNil(); // never executes
}

struct func { char* name; naCFunction func; };
static struct func funcs[] = {
    { "size", size },
    { "keys", keys }, 
    { "append", append }, 
    { "pop", pop }, 
    { "setsize", setsize }, 
    { "subvec", subvec }, 
    { "delete", delete }, 
    { "int", intf },
    { "num", num },
    { "streq", streq },
    { "substr", substr },
    { "contains", contains },
    { "typeof", typeOf },
    { "compile", f_compile },
    { "call", f_call },
    { "die", f_die },
};

naRef naStdLib(naContext c)
{
    naRef namespace = naNewHash(c);
    int i, n = sizeof(funcs)/sizeof(struct func);
    for(i=0; i<n; i++) {
        naRef code = naNewCCode(c, funcs[i].func);
        naRef name = naStr_fromdata(naNewString(c),
                                    funcs[i].name, strlen(funcs[i].name));
        name = naInternSymbol(name);
        naHash_set(namespace, name, naNewFunc(c, code));
    }
    return namespace;
}
