
#include <math.h>
#include <string.h>

#include "nasal.h"

static naRef f_sin(naContext c, naRef me, int argc, naRef* args)
{
    naRef a = naNumValue(argc > 0 ? args[0] : naNil());
    if(naIsNil(a))
        naRuntimeError(c, "non numeric argument to sin()");
    a.num = sin(a.num);
    return a;
}

static naRef f_cos(naContext c, naRef me, int argc, naRef* args)
{
    naRef a = naNumValue(argc > 0 ? args[0] : naNil());
    if(naIsNil(a))
        naRuntimeError(c, "non numeric argument to cos()");
    a.num = cos(a.num);
    return a;
}

static naRef f_exp(naContext c, naRef me, int argc, naRef* args)
{
    naRef a = naNumValue(argc > 0 ? args[0] : naNil());
    if(naIsNil(a))
        naRuntimeError(c, "non numeric argument to exp()");
    a.num = exp(a.num);
    return a;
}

static naRef f_ln(naContext c, naRef me, int argc, naRef* args)
{
    naRef a = naNumValue(argc > 0 ? args[0] : naNil());
    if(naIsNil(a))
        naRuntimeError(c, "non numeric argument to ln()");
    a.num = log(a.num);
    return a;
}

static naRef f_sqrt(naContext c, naRef me, int argc, naRef* args)
{
    naRef a = naNumValue(argc > 0 ? args[0] : naNil());
    if(naIsNil(a))
        naRuntimeError(c, "non numeric argument to sqrt()");
    a.num = sqrt(a.num);
    return a;
}

static naRef f_atan2(naContext c, naRef me, int argc, naRef* args)
{
    naRef a = naNumValue(argc > 0 ? args[0] : naNil());
    naRef b = naNumValue(argc > 1 ? args[1] : naNil());
    if(naIsNil(a) || naIsNil(b))
        naRuntimeError(c, "non numeric argument to atan2()");
    a.num = atan2(a.num, b.num);
    return a;
}

static struct func { char* name; naCFunction func; } funcs[] = {
    { "sin", f_sin },
    { "cos", f_cos },
    { "exp", f_exp },
    { "ln", f_ln },
    { "sqrt", f_sqrt },
    { "atan2", f_atan2 },
};

naRef naMathLib(naContext c)
{
    naRef name, namespace = naNewHash(c);
    int i, n = sizeof(funcs)/sizeof(struct func);
    for(i=0; i<n; i++) {
        naRef code = naNewCCode(c, funcs[i].func);
        naRef name = naStr_fromdata(naNewString(c),
                                    funcs[i].name, strlen(funcs[i].name));
        naHash_set(namespace, name, naNewFunc(c, code));
    }

    // Set up constants for math.pi and math.e.  Can't use M_PI or
    // M_E, becuase those aren't technically part of the C standard.  Sigh.
    name = naStr_fromdata(naNewString(c), "pi", 2);
    naHash_set(namespace, name, naNum(3.14159265358979323846));

    name = naStr_fromdata(naNewString(c), "e", 1);
    name = naInternSymbol(name);
    naHash_set(namespace, name, naNum(2.7182818284590452354));

    return namespace;
}
