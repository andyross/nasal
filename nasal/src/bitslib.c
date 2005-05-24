#include <string.h>
#include "data.h"

// Note that this currently supports a maximum field width of 32
// bits (i.e. an unsigned int).  Using a 64 bit integer would stretch
// that beyond what is representable in the double result, but
// requires portability work.

#define BIT(s,l,n) s[l-1-((n)>>3)] & (1<<((n)&7))
#define CLRB(s,l,n) s[l-1-((n)>>3)] &= ~(1<<((n)&7))
#define SETB(s,l,n) s[l-1-((n)>>3)] |= 1<<((n)&7)

static unsigned int fld(naContext c, unsigned char* s,
                        int slen, int bit, int flen)
{
    int i;
    unsigned int fld = 0;
    if(bit + flen > 8*slen) naRuntimeError(c, "bitfield out of bounds");
    for(i=0; i<flen; i++) if(BIT(s, slen, i+bit)) fld |= (1<<i);
    return fld;
}

static void setfld(naContext c, unsigned char* s, int slen,
                   int bit, int flen, unsigned int fld)
{
    int i;
    if(bit + flen > 8*slen) naRuntimeError(c, "bitfield out of bounds");
    for(i=0; i<flen; i++)
        if(fld & (1<<i)) SETB(s, slen, i+bit);
        else CLRB(s, slen, i+bit);
}

static naRef dofld(naContext c, int argc, naRef* args, int sign)
{
    struct naStr* s = argc > 0 ? args[0].ref.ptr.str : 0;
    int bit = argc > 1 ? (int)naNumValue(args[1]).num : -1;
    int len = argc > 2 ? (int)naNumValue(args[2]).num : -1;
    unsigned int f;
    if(!s || !MUTABLE(args[0]) || bit < 0 || len < 0)
        naRuntimeError(c, "missing/bad argument to fld/sfld");
    f = fld(c, s->data, s->len, bit, len);
    if(!sign) return naNum(f);
    if(f & (1 << (len-1))) f |= ~((1<<len)-1); // sign extend
    return naNum((signed int)f);
}

static naRef f_sfld(naContext c, naRef me, int argc, naRef* args)
{
    return dofld(c, argc, args, 1);
}

static naRef f_fld(naContext c, naRef me, int argc, naRef* args)
{
    return dofld(c, argc, args, 0);
}

static naRef f_setfld(naContext c, naRef me, int argc, naRef* args)
{
    struct naStr* s = argc > 0 ? args[0].ref.ptr.str : 0;
    int bit = argc > 1 ? (int)naNumValue(args[1]).num : -1;
    int len = argc > 2 ? (int)naNumValue(args[2]).num : -1;
    naRef val = argc > 3 ? naNumValue(args[3]) : naNil();
    if(!argc || !MUTABLE(args[0])|| bit < 0 || len < 0 || IS_NIL(val))
        naRuntimeError(c, "missing/bad argument to setfld");
    setfld(c, s->data, s->len, bit, len, (unsigned int)val.num);
    return naNil();
}

static naRef f_buf(naContext c, naRef me, int argc, naRef* args)
{
    naRef len = argc ? naNumValue(args[0]) : naNil();
    if(IS_NIL(len)) naRuntimeError(c, "missing/bad argument to buf");
    return naStr_buf(naNewString(c), (int)len.num);
}

static struct func { char* name; naCFunction func; } funcs[] = {
    { "sfld", f_sfld },
    { "fld", f_fld },
    { "setfld", f_setfld },
    { "buf", f_buf },
};

naRef naBitsLib(naContext c)
{
    naRef namespace = naNewHash(c);
    int i, n = sizeof(funcs)/sizeof(struct func);
    for(i=0; i<n; i++) {
        naRef code = naNewCCode(c, funcs[i].func);
        naRef name = naStr_fromdata(naNewString(c),
                                    funcs[i].name, strlen(funcs[i].name));
        naHash_set(namespace, name, naNewFunc(c, code));
    }
    return namespace;
}
