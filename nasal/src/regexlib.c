#include <pcre.h>
#include <string.h>
#include "data.h"

static void regexDestroy(void* r);
naGhostType naRegexGhostType = { regexDestroy };

struct Regex {
    pcre* re;
    pcre_extra* extra;
};

static int parseOpts(naContext c, char* s)
{
    int opts = 0;
    while(*s) {
        switch(*s++) {
        case 'i': opts |= PCRE_CASELESS; break;
        case 'm': opts |= PCRE_MULTILINE; break;
        case 's': opts |= PCRE_DOTALL; break;
        case 'x': opts |= PCRE_EXTENDED; break;
        default: naRuntimeError(c, "unrecognized regex option");
        }
    }
    return opts;
}

static naRef f_compile(naContext c, naRef me, int argc, naRef* args)
{
    int erroff, opts = 0;
    const char* errptr;
    struct Regex* regex;
    naRef pat = argc > 0 ? naStringValue(c, args[0]) : naNil();
    naRef optr = argc > 1 ? args[1] : naNil();
    if(!IS_STR(pat) || (!IS_NIL(optr) && !IS_STR(optr)))
        naRuntimeError(c, "bad arg to regex.compile");
    if(IS_STR(optr)) opts = parseOpts(c, (char*)optr.ref.ptr.str->data);
    regex = naAlloc(sizeof(struct Regex));
    regex->re = pcre_compile((char*)pat.ref.ptr.str->data, opts, &errptr, &erroff, 0);
    if(regex->re == 0) {
        naFree(regex);
        naRuntimeError(c, "bad regex"); // FIXME: expose errptr/erroff
    }
    regex->extra = pcre_study(regex->re, 0, &errptr);
    return naNewGhost(c, &naRegexGhostType, regex);
}

static naRef f_exec(naContext c, naRef me, int argc, naRef* args)
{
    int i, n, ovector[30];
    struct Regex* r = argc > 0 ? naGhost_ptr(args[0]) : 0;
    naRef result, str = argc > 1 ? naStringValue(c, args[1]) : naNil();
    naRef start = argc > 2 ? naNumValue(args[2]) : naNum(0);
    if(!r || naGhost_type(args[0]) != &naRegexGhostType || !IS_NUM(start))
        naRuntimeError(c, "bad argument to regex.study");
    n = pcre_exec(r->re, r->extra, (char*)str.ref.ptr.str->data,
                  str.ref.ptr.str->len, (int)start.num, 0, ovector, 30);
    result = naNewVector(c);
    if(n > 0) {
        naVec_setsize(result, n*2);
        for(i=0; i<2*n; i++) naVec_set(result, i, naNum(ovector[i]));
    }
    return result;
}

static void regexDestroy(void* r)
{
    struct Regex* regex = (struct Regex*)r;
    pcre_free(regex->re);
    if(regex->extra) pcre_free(regex->extra);
    naFree(regex);
}

static struct func { char* name; naCFunction func; } funcs[] = {
    { "compile", f_compile },
    { "exec", f_exec },
};

static void setsym(naContext c, naRef hash, char* sym, naRef val)
{
    naRef name = naStr_fromdata(naNewString(c), sym, strlen(sym));
    naHash_set(hash, naInternSymbol(name), val);
}

naRef naRegexLib(naContext c)
{
    naRef ns = naNewHash(c);
    int i, n = sizeof(funcs)/sizeof(struct func);
    for(i=0; i<n; i++)
        setsym(c, ns, funcs[i].name,
               naNewFunc(c, naNewCCode(c, funcs[i].func)));
    return ns;
}
