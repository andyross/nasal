#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

#include "config.h"
#include "nasal.h"
#include "data.h"

void checkError(naContext ctx)
{
    int i;
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
}


#ifdef _WIN32
DWORD WINAPI threadtop(LPVOID param)
#else
void* threadtop(void* param)
#endif
{
    naContext ctx = naNewContext();
    naRef func = naNil();
    SETPTR(func, param);
    naCall(ctx, func, 0, 0, naNil(), naNil());
    checkError(ctx);
    naFreeContext(ctx);
    return 0;
}

// A brutally simple "create a thread" API, taking a single function.
// Useful for test purposes, but for little else.  Note that there are
// no synchronization primitives defined!
static naRef newthread(naContext c, naRef me, int argc, naRef* args)
{
#ifndef _WIN32
    pthread_t th;
    naSave(c, args[0]);
    pthread_create(&th, 0, threadtop, PTR(args[0]).obj);
#else
    naSave(c, args[0]);
    CreateThread(0, 0, threadtop, PTR(args[0]).obj, 0, 0);
#endif
    return naNil();
}

// A Nasal extension function (prints its argument list to stdout)
static naRef print(naContext c, naRef me, int argc, naRef* args)
{
    int i;
    for(i=0; i<argc; i++) {
        naRef s = naStringValue(c, args[i]);
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
    naRef code, namespace, result, *args;
    int errLine, i;

    if(argc < 2) {
        fprintf(stderr, "nasal: must specify a script to run\n");
        exit(1);
    }
    script = argv[1];

    // Read the contents of the file into a buffer in memory.
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
    // will be the namespace for a new script
    namespace = naInit_std(ctx);

    // Add application-specific functions (in this case, "print" and
    // the math library) to the namespace if desired.
    naHash_set(namespace, naInternSymbol(NASTR("print")),
               naNewFunc(ctx, naNewCCode(ctx, print)));
    naHash_set(namespace, naInternSymbol(NASTR("thread")),
               naNewFunc(ctx, naNewCCode(ctx, newthread)));

    // Add extra libraries as needed.
    naHash_set(namespace, naInternSymbol(NASTR("utf8")), naInit_utf8(ctx));
    naHash_set(namespace, naInternSymbol(NASTR("math")), naInit_math(ctx));
    naHash_set(namespace, naInternSymbol(NASTR("bits")), naInit_bits(ctx));
    naHash_set(namespace, naInternSymbol(NASTR("io")), naInit_io(ctx));
    naHash_set(namespace, naInternSymbol(NASTR("unix")), naInit_unix(ctx));
#ifdef HAVE_PCRE
    naHash_set(namespace, naInternSymbol(NASTR("regex")), naInit_regex(ctx));
#endif
#ifdef HAVE_SQLITE
    naHash_set(namespace, naInternSymbol(NASTR("sqlite")), naInit_sqlite(ctx));
#endif
#ifdef HAVE_READLINE
    naHash_set(namespace, naInternSymbol(NASTR("readline")),
               naInit_readline(ctx));
#endif

    // Bind the "code" object from naParseCode into a "function"
    // object.  This is optional, really -- we could also just pass
    // the namespace hash as the final argument to naCall().  But
    // having the global namespace in an outer scope means that we
    // won't be polluting it with the local variables of the script.
    code = naBindFunction(ctx, code, namespace);

    // Build the arg vector
    args = malloc(sizeof(naRef) * (argc-2));
    for(i=0; i<argc-2; i++)
        args[i] = NASTR(argv[i+2]);

    // Run it.  Do something with the result if you like.
    result = naCall(ctx, code, argc-2, args, naNil(), naNil());

    free(args);

    checkError(ctx);
    return 0;
}
#undef NASTR
