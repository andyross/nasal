#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "nasl.h"

// A Nasl extension function (prints its argument list to stdout)
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

int main(int argc, char** argv)
{
    FILE* f;
    struct stat fdat;
    char* buf;
    struct Context *ctx;
    naRef code, namespace, result;
    int parseError = 0;

    if(argc < 2) {
        fprintf(stderr, "nasl: must specify a script to run\n");
        exit(1);
    }

    // Read the contents of the file into a buffer in memory.
    // Unix-only stat() call; use cygwin if you're in windows, I
    // guess.  Does the C library provide a way to do this at all?
    f = fopen(argv[1], "r");
    if(!f) {
        fprintf(stderr, "nasl: could not open input file: %s\n", argv[1]);
        exit(1);
    }
    stat(argv[1], &fdat);
    buf = malloc(fdat.st_size);
    if(fread(buf, 1, fdat.st_size, f) != fdat.st_size) {
        fprintf(stderr, "nasl: error in fread()\n");
        exit(1);
    }
    
    // Create an interpreter context
    ctx = naNewContext();

    // Parse the code in the buffer.  The line of a fatal parse error
    // is returned via the pointer.
    code = naParseCode(ctx, buf, fdat.st_size, &parseError);
    if(parseError) {
        fprintf(stderr, "Parse error: %s at line %d\n",
                naGetError(ctx), parseError);
        exit(1);
    }
    free(buf);

    // Make a hash containing the standard library functions.  This
    // will be the namespace for a new script (more elaborate
    // environments -- imported libraries, for example -- might be
    // different).
    namespace = naStdLib(ctx);

    // Add application-specific functions (in this case, "print") to
    // the namespace if desired.
    naHash_set(namespace,
               naStr_fromdata(naNewString(ctx), "print", 5),
               naNewFunc(ctx,
                         naNewCCode(ctx, print), // CCODE object
                         naNil())); // function closure (none here)

    // Run it.  Do something with the result if you like.
    result = naCall(ctx, code, namespace);

    // Did it fail? (FIXME: replace with stack trace)
    if(naGetError(ctx)) {
        fprintf(stderr, "Runtime error: %s at line %d\n",
                naGetError(ctx), naCurrentLine(ctx));
        exit(1);
    }
    
    return 0;
}
