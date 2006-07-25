#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <readline/readline.h>
#include <readline/history.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

#include "nasal.h"

void dumpByteCode(naRef codeObj);
/*
//uncomment this if dumpByteCode is not in debug.c
void dumpByteCode(naRef codeObj) {
    unsigned short *byteCode = codeObj.ref.ptr.code->byteCode;
    int ip = 0, op, c;
    naRef a;
    while(ip < codeObj.ref.ptr.code->codesz) {
        op = byteCode[ip++];
        printf("%8d %-12s",ip-1,opStringDEBUG(op));
        switch(op) {
            case OP_PUSHCONST: case OP_MEMBER: case OP_LOCAL:
                c=byteCode[ip++];
                a=codeObj.ref.ptr.code->constants[c];
                printf(" %-4d ",c);
                if(IS_CODE(a)) {
                    printf("(CODE)\n[\n");
                    dumpByteCode(a);
                    printf("]\n");
                } else
                    printRefDEBUG(a);
            break;
            case OP_JIFNOT: case OP_JIFNIL: case OP_JMP: case OP_JMPLOOP:
            case OP_FCALL: case OP_MCALL: case OP_FTAIL: case OP_MTAIL:
                printf(" %d\n",byteCode[ip++]);
            break;
            default:
                printf("\n");
        }
    }
}
*/

int do_list = 0;
naRef namespace, candidate;

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

int printRef(naContext ctx, naRef r, int max)
{
    int i;
	static int count;
	if(max) count=max;
	else
	if(count == 0) {
		printf("...");
		return 1;
	}

	count--;
	
    if(naIsNum(r)) {
        printf("%g", r.num);
    } else if(naIsNil(r)) {
        printf("<nil>");
    } else if(naIsString(r)) {
        printf("\"%s\"",naStr_data(r));
    } else if(naIsVector(r)) {
        int i,sz=naVec_size(r);
        printf("[");
        for(i=0;i<sz;i++) {
            if(printRef(ctx,naVec_get(r,i),0)) break;
            if(i<sz-1) printf(", ");
        }
        printf("]");
    } else if(naIsHash(r)) {
        int i,sz=naHash_size(r);
        naRef keys = naNewVector(ctx);
        naHash_keys(keys,r);
        printf("{");
        for(i=0;i<sz;i++) {
            naRef val,key=naVec_get(keys,i);
            if(printRef(ctx,key,0)) break;
            printf(" : ");
            naHash_get(r,key,&val);
            printRef(ctx,val,0);
            if(i<sz-1) printf(", ");
        }       
        printf("}");
    } else if(naIsFunc(r)) {
        printf("<func> %p",r.ref.ptr.func);
    } else if(naIsCode(r)) {
        printf("<code> %p",r.ref.ptr.code);
    } else if(naIsGhost(r)) {
        printf("<ghost> %p",r.ref.ptr.ghost);
    }
	return 0;
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

    ns = namespace;
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

#define NASTR(s) naStr_fromdata(naNewString(ctx), (s), strlen((s)))

void naInteractive(naContext ctx, naRef namespace) {
    int level=0;
    char prompt[] = ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> ";
    char *text=NULL,*buf;
    int escape=0;
    int in_string=0;
    int errLine;
    naRef code,result;

    printf("Press Ctrl-D to exit nasal interpreter\n");
    while(1) {
        char *s;
        s=prompt+strlen(prompt)-level-2;
        if(s<prompt) s=prompt;
        buf = readline(s);
        if(!buf) break;

        s=buf;
        while(*s) {
            if(!escape && *s == '"') {
                if(in_string==2) {
                    in_string=0;
                    level--;
                } else
                if(in_string==0) {
                    in_string=2;
                    level++;
                }
            }
            else
            if(!escape && *s == '\'') {
                if(!escape && in_string==1) {
                    in_string=0;
                    level--;
                } else
                if(in_string==0) {
                    in_string=1;
                    level++;
                }
            }
            else
            if(!(in_string || escape) && strchr("{[(",*s)) level++;
            else
            if(!(in_string || escape) && strchr("}])",*s)) level--;
            escape=0;
            if(*s == '\\') escape=1;
            else
            if(!in_string && *s == '`') escape=1;
            s++;
        }
        if(level<0) level=0;

        if(text) {
            int i = strlen(text);
            text = realloc(text,i+strlen(buf)+1);
            strcpy(text+i,buf);
        } else
            text=strdup(buf);

        if(level==0) {
            code = naParseCode(ctx, naNil(), 1, text, strlen(text), &errLine);
            free(text);
            text=NULL;
        }
        add_history(buf);
        free(buf);

        if(level==0) {
            if(naIsNil(code)) {
                fprintf(stderr, "Parse error: %s\n", naGetError(ctx));
                continue;
            }
            if(do_list)
                dumpByteCode(code);
            else {
                result = naCall(ctx, code, 0, NULL, naNil(), namespace);
				naHash_delete(namespace, naInternSymbol(NASTR("arg")));
                
				if(naGetError(ctx))
                    fprintf(stderr, "Runtime error: %s\n",naGetError(ctx));
                else {
                    printRef(ctx,result,500);
                    printf("\n");
                }
            }
        }
    }
}

void naScriptfile(naContext ctx, char *script, int argc, char **argv, naRef namespace)
{
    FILE* f;
    struct stat fdat;
    char *buf;
    naRef code, result, *args;
    int errLine, i;

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

    code = naParseCode(ctx, NASTR(script), 1, buf, fdat.st_size, &errLine);
    free(buf);
    if(naIsNil(code)) {
        fprintf(stderr, "Parse error: %s at line %d\n",
                naGetError(ctx), errLine);
        exit(1);
    }

    if(do_list) {
        dumpByteCode(code);
    } else {
        args = malloc(sizeof(naRef) * (argc));
        for(i=0; i<argc; i++)
            args[i] = NASTR(argv[i]);
        result = naCall(ctx, code, argc, args, naNil(), namespace);
        printf("[Result: ");
        printRef(ctx,result,1000);
        printf(" ]\n");
        free(args);
        checkError(ctx);
    }
}

int main(int argc, char** argv)
{
    char *script;
    naContext ctx;
    argv++; argc--;
    
    if(argc && strcmp(*argv,"--list")==0) {
        argv++; argc--;
        do_list = 1;
    }

    if(argc < 1) {
        script = NULL;
    } else {
        script = *argv++;
        argc--;
    }

    rl_completion_entry_function = (int (*)(const char*, int))name_generator;
    rl_completer_word_break_characters = " \t\n\"\\'`@$><=;,|&{}()[]+-/*";
    rl_completion_append_character = '\0';
    
    ctx = naNewContext();
    
    namespace = naInit_std(ctx);
    naSave(ctx,namespace); //is this needed?
    naHash_set(namespace, naInternSymbol(NASTR("print")),
               naNewFunc(ctx, naNewCCode(ctx, print)));
    naHash_set(namespace, naInternSymbol(NASTR("thread")),
               naNewFunc(ctx, naNewCCode(ctx, newthread)));
    naHash_set(namespace, naInternSymbol(NASTR("utf8")), naInit_utf8(ctx));
    naHash_set(namespace, naInternSymbol(NASTR("math")), naInit_math(ctx));
    naHash_set(namespace, naInternSymbol(NASTR("bits")), naInit_bits(ctx));
    naHash_set(namespace, naInternSymbol(NASTR("io")), naInit_io(ctx));
    naHash_set(namespace, naInternSymbol(NASTR("unix")), naInit_unix(ctx));
    naHash_set(namespace, naInternSymbol(NASTR("regex")), naInit_regex(ctx));

    candidate = naNewVector(ctx);
    naSave(ctx,candidate); //is this needed?
    
    if(script)
        naScriptfile(ctx,script,argc,argv,namespace);
    else
        naInteractive(ctx,namespace);
        
    return 0;
}
#undef NASTR
