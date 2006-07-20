/*
 * Interactive (readline-based) Nasal interpreter, with command line
 * editing, history, and even tab-completion of Nasal symbols!
 * Written by Manabu Nishiyama and Jonatan Liljedahl
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <readline/readline.h>


#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

#include "nasal.h"

naRef nspace, candidate;

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
    func.ref.ptr.func = param;
    naCall(ctx, func, 0, 0, naNil(), naNil());
    checkError(ctx);
    naFreeContext(ctx);
    return 0;
}

// A brutally simple "create a thread" API, taking a single function.
// Useful for test purposes, but for little else.
static naRef newthread(naContext c, naRef me, int argc, naRef* args)
{
#ifndef _WIN32
    pthread_t th;
    naSave(c, args[0]);
    pthread_create(&th, 0, threadtop, args[0].ref.ptr.obj);
#else
    naSave(c, args[0]);
    CreateThread(0, 0, threadtop, args[0].ref.ptr.obj, 0, 0);
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

char *name_generator(const char *text, int state)
{
    static naRef ns;
    static int list_index, len, pos;
    char *name;

    /* If this is a new word to complete, initialize now.  This
       includes saving the length of TEXT for efficiency, and
       initializing the index variable to 0. */

    if (!state){
        char *dtext = strdup(text), *start = dtext, *end;

	ns = nspace;
	pos = 0;
	while(naVec_size(candidate)) naVec_removelast(candidate);
	while((end = strchr(start, '.'))){
	    *end = '\0';
	    ns = naHash_cget(ns, start);
	    pos = end-dtext+1;
	    *end = '.';
	    start = end+1;
	    if (!naIsHash(ns)){
	        break;
	    }
	}
	free(dtext);

	list_index = 0;
	len = strlen(text+pos);
	naHash_keys(candidate, ns);
    }
    /* Return the next name which partially matches from the
       command list. */
    while((name = naStr_data(naVec_get(candidate, list_index)))){
        list_index++;

	if (strncmp (name, text+pos, len) == 0){
	    //Allocated memory is to be freed by Readline library.
	    char *p = (char *)malloc(strlen(text)+strlen(name)-len+1);
	  
	    strcpy(p, text);
	    p[pos] = '\0';
	    strcat(p, name);
	    strcat(p, naIsHash((naHash_cget(ns, name))) ? ".":"\0");
	    return p;
	}
    }
    
    /* If no names matched, then return NULL. */
    return (char *)0;
}

#define NASTR(s) naStr_fromdata(naNewString(ctx), (s), strlen((s)))
int main(int argc, char** argv)
{
    char *buf, *text=NULL, *script = "(interactive mode)";
    struct Context *ctx;
    naRef code, result, sresult;
    int errLine, i;
    int cont = 0;

    /* Tell the GNU Readline library how to complete. */
    rl_completion_entry_function = (int (*)(const char*, int))name_generator;
    rl_completer_word_break_characters = " \t\n\"\\'`@$><=;,|&{}()[]+-/*";
    rl_completion_append_character = '\0';
    
    // Create an interpreter context
    ctx = naNewContext();

    // Make a hash containing the standard library functions.  This
    // will be the namespace for a new script (more elaborate
    // environments -- imported libraries, for example -- might be
    // different).
    nspace = naStdLib(ctx);
    naSave(ctx, nspace);
    
    // Add application-specific functions (in this case, "print" and
    // the math library) to the namespace if desired.
    naHash_set(nspace, naInternSymbol(NASTR("print")),
               naNewFunc(ctx, naNewCCode(ctx, print)));
    naHash_set(nspace, naInternSymbol(NASTR("thread")),
               naNewFunc(ctx, naNewCCode(ctx, newthread)));

    // Add extra libraries as needed.
    naHash_set(nspace, naInternSymbol(NASTR("math")), naMathLib(ctx));
    naHash_set(nspace, naInternSymbol(NASTR("bits")), naBitsLib(ctx));
    naHash_set(nspace, naInternSymbol(NASTR("io")), naIOLib(ctx));
    //I have not installed PCRE yet...
    //naHash_set(nspace, naInternSymbol(NASTR("regex")), naRegexLib(ctx));
    naHash_set(nspace, naInternSymbol(NASTR("unix")), naUnixLib(ctx));

    candidate = naNewVector(ctx);
    naSave(ctx, candidate);
    
    cont = 0;
    while(1){
        // Read one line.
	if(cont == 0){
	    buf = readline("Nasal: ");
	}else{
	    buf = readline(".....: ");
	}
	
	if (buf){
	    if(*buf != '\0') add_history (buf);
	}else{
	    break;
	}
	if(!strcmp("exit", buf)) break;

	if(text != NULL) {
	    int i = strlen(text);
	    text = realloc(text,i+strlen(buf)+1);
	    strcpy(text+i,buf);
	}else{
	    text = strdup(buf);
	}
	
	free(buf);

	// Parse the code in the buffer.  The line of a fatal parse error
	// is returned via the pointer.
	code = naParseCode(ctx, NASTR(script), 1, text, strlen(text), &errLine);
	if(naIsNil(code)) {
	    // Decide program is continued or not
	    if(strcmp(naGetError(ctx), "unterminated brace") == 0){
		cont = 1;
	    }else{
		fprintf(stderr, "Parse error: %s at line %d\n",
			naGetError(ctx), errLine);
		free(text);
		text = NULL;
		cont = 0;
	    }
	    continue;
	}
	free(text);
	text = NULL;
	cont = 0;
	
	// Run it.  Do something with the result if you like.
	result = naCall(ctx, code, 0, 0, naNil(), nspace);
	naHash_delete(nspace, naInternSymbol(NASTR("arg")));

	// Did it fail? Print a nice warning, with stack trace
	// information.
	if(naGetError(ctx)) {
	    fprintf(stderr, "Runtime error: %s\n", naGetError(ctx));

	    for(i=1; i<naStackDepth(ctx); i++)
		fprintf(stderr, "  called from: %s\n",
			naStr_data(naGetSourceFile(ctx, i)));
	    continue;
	}
	// Display result.
	sresult = naStringValue(ctx, result);
	if(naIsNil(sresult)) continue;
	fwrite(naStr_data(sresult), 1, naStr_len(sresult), stdout);
	fprintf(stdout, "\n");
    }
    
    return 0;
}
#undef NASTR
