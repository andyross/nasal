#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "nasal.h"

// A Nasal extension function (prints its argument list to stdout)
static naRef print(naContext c, naRef args)
{
    int i, n;
    n = naVec_size(args);
    for(i=0; i<n; i++) {
        naRef s = naStringValue(c, naVec_get(args, i));
        if(naIsNil(s)) continue;
        fwrite(naStr_data(s), 1, naStr_len(s), stdout);
    }
    return naNil();
}

#define NASTR(s) naStr_fromdata(naNewString(ctx), (s), strlen((s)))
int main(int argc, char** argv)
{
    FILE* f;
    struct stat fdat;
    char *buf, *script;
    struct Context *ctx;
    naRef code, namespace, result;
    int errLine, i;

    if(argc < 2) {
        fprintf(stderr, "nasal: must specify a script to run\n");
        exit(1);
    }
    script = argv[1];

    // Read the contents of the file into a buffer in memory.
    // Unix-only stat() call; use cygwin if you're in windows, I
    // guess.  Does the C library provide a way to do this at all?
    f = fopen(script, "r");
    if(!f) {
        fprintf(stderr, "nasal: could not open input file: %s\n", script);
        exit(1);
    }
    stat(script, &fdat);
    buf = malloc(fdat.st_size);
    if(fread(buf, 1, fdat.st_size, f) != fdat.st_size) {
        fprintf(stderr, "nasal: error in fread()\n");
        exit(1);
    }
    
    // Create an interpreter context
    ctx = naNewContext();

    // Parse the code in the buffer.  The line of a fatal parse error
    // is returned via the pointer.
    code = naParseCode(ctx, NASTR(script), 1, buf, fdat.st_size, &errLine);
    if(naIsNil(code)) {
        fprintf(stderr, "Parse error: %s at line %d\n",
                naGetError(ctx), errLine);
        exit(1);
    }
    free(buf);

    // Make a hash containing the standard library functions.  This
    // will be the namespace for a new script (more elaborate
    // environments -- imported libraries, for example -- might be
    // different).
    namespace = naStdLib(ctx);
    naHash_set(namespace, NASTR("math"), naMathLib(ctx));

    // Add application-specific functions (in this case, "print" and
    // the math library) to the namespace if desired.
    naHash_set(namespace, NASTR("print"),
               naNewFunc(ctx, naNewCCode(ctx, print)));

    // Run it.  Do something with the result if you like.
    result = naCall(ctx, code, naNil(), naNil(), namespace);

    // Did it fail? Print a nice warning, with stack trace
    // information.
    if(naGetError(ctx)) {
        fprintf(stderr, "Runtime error: %s\n  at %s, line %d\n",
                naGetError(ctx), naStr_data(naGetSourceFile(ctx, 0)),
                naGetLine(ctx, 0));

        for(i=1; i<naStackDepth(ctx); i++)
            fprintf(stderr, "  called from: %s, line %d\n",
                    naStr_data(naGetSourceFile(ctx, i)),
                    naGetLine(ctx, i));
        exit(1);
    }
    
    return 0;
}
#undef NASTR
