#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef _MSC_VER // sigh...
#define vsnprintf _vsnprintf
#endif

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

// Wrapper around vsnprintf, iteratively increasing the buffer size
// until it fits.  Returned buffer should be freed by the caller.
char* dosprintf(char* f, ...)
{
    char* buf;
    va_list va;
    int len = 16;
    while(1) {
        buf = naAlloc(len);
        va_start(va, f);
        if(vsnprintf(buf, len, f, va) < len) {
            va_end(va);
            return buf;
        }
        va_end(va);
        naFree(buf);
        len *= 2;
    }
}

// Inspects a printf format string f, and finds the next "%..." format
// specifier.  Stores the start of the specifier in out, the length in
// len, and the type in type.  Returns a pointer to the remainder of
// the format string, or 0 if no format string was found.  Recognizes
// all of ANSI C's syntax except for the "length modifier" feature.
// Note: this does not validate the format character returned in
// "type". That is the caller's job.
static char* nextFormat(naContext ctx, char* f, char** out, int* len, char* type)
{
    // Skip to the start of the format string
    while(*f && *f != '%') f++;
    if(!*f) return 0;
    *out = f++;

    while(*f && (*f=='-' || *f=='+' || *f==' ' || *f=='0' || *f=='#')) f++;

    // Test for duplicate flags.  This is pure pedantry and could
    // be removed on all known platforms, but just to be safe...
    {   char *p1, *p2;
        for(p1 = *out + 1; p1 < f; p1++)
            for(p2 = p1+1; p2 < f; p2++)
                if(*p1 == *p2)
                    naRuntimeError(ctx, "duplicate flag in format string"); }

    while(*f && *f >= '0' && *f <= '9') f++;
    if(*f && *f == '.') f++;
    while(*f && *f >= '0' && *f <= '9') f++;
    if(!*f) naRuntimeError(ctx, "invalid format string");

    *type = *f++;
    *len = f - *out;
    return f;
}

#define NEWSTR(s, l) naStr_fromdata(naNewString(ctx), s, l)
#define ERR(m) naRuntimeError(ctx, m)
#define APPEND(r) result = naStr_concat(naNewString(ctx), result, r)
static naRef f_sprintf(naContext ctx, naRef me, naRef args)
{
    char t, nultmp, *fstr, *next, *fout, *s;
    int flen, argn=1;
    naRef format, arg, result = naNewString(ctx);

    if(naVec_size(args) < 1) ERR("not enough arguments to sprintf");
    format = naStringValue(ctx, naVec_get(args, 0));
    if(naIsNil(format)) ERR("bad format string in sprintf");
    s = naStr_data(format);
                               
    while((next = nextFormat(ctx, s, &fstr, &flen, &t))) {
        APPEND(NEWSTR(s, fstr-s)); // stuff before the format string
        if(argn >= naVec_size(args)) ERR("not enough arguments to sprintf");
        arg = naVec_get(args, argn++);
        nultmp = fstr[flen]; // sneaky nul termination...
        fstr[flen] = 0;
        if(t == 's') {
            arg = naStringValue(ctx, arg);
            if(naIsNil(arg)) fout = dosprintf(fstr, "nil");
            else             fout = dosprintf(fstr, naStr_data(arg));
        } else {
            arg = naNumValue(arg);
            if(naIsNil(arg))
                fout = dosprintf(fstr, "nil");
            else if(t=='d' || t=='i' || t=='c')
                fout = dosprintf(fstr, (int)naNumValue(arg).num);
            else if(t=='o' || t=='u' || t=='x' || t=='X')
                fout = dosprintf(fstr, (unsigned int)naNumValue(arg).num);
            else if(t=='e' || t=='E' || t=='f' || t=='F' || t=='g' || t=='G')
                fout = dosprintf(fstr, naNumValue(arg).num);
            else
                ERR("invalid sprintf format type");
        }
        fstr[flen] = nultmp;
        APPEND(NEWSTR(fout, strlen(fout)));
        naFree(fout);
        s = next;
    }
    APPEND(NEWSTR(s, strlen(s)));
    return result;
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
    { "sprintf", f_sprintf },
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
