#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "nasal.h"
#include "code.h"

static naRef completion_func;
static naContext curr_ctx;
static naRef keep_this;

#define NASTR(s) naStr_fromdata(naNewString(ctx), (s), strlen((s)))

static naRef arg_str(naContext c, naRef *a,int n, char *f) {
    naRef r = naStringValue(c,a[n]);
    if(!IS_STR(r))
        naRuntimeError(c,"Arg %d to %s() not a string",n+1,f);
    return r;
}
static naRef arg_func(naContext c, naRef *a,int n, char *f) {
    if(!IS_FUNC(a[n]))
        naRuntimeError(c,"Arg %d to %s() not a func",n+1,f);
    return a[n];
}
static double arg_num(naContext c, naRef *a,int n, char *f) {
    naRef r = naNumValue(a[n]);
    if(!IS_NUM(r))
        naRuntimeError(c,"Arg %d to %s() not a num",n+1,f);
    return r.num;
}
static void check_argc(naContext c, int argc, int n, char *f) {
    if(argc!=n) naRuntimeError(c,"%s() takes %d args, not %d",f,n,argc);
}

static char *completion_wrapper(const char *text, int state)
{
    naContext ctx;
    naRef result, args[2];
    char *s=NULL;
    
    if(!IS_FUNC(completion_func))
        return NULL;
    ctx = naSubContext(curr_ctx);
    args[0] = NASTR((char*)text);
    args[1] = naNum(state);
    result = naCall(ctx,completion_func,2,args,naNil(),naNil());
    if(naGetError(ctx)) naRethrowError(ctx);
    if(IS_STR(result)) s = naStr_data(result);
    naFreeContext(ctx);
    if(s!=NULL) s=strdup(s); //readline should free it, so we must dup it.
    return s;
}

static naRef f_readline(naContext ctx, naRef me, int argc, naRef* args)
{
    char *p, *buf, *f = "readline";
    if(argc>1) check_argc(ctx,argc,1,f);
    p = argc>0?naStr_data(arg_str(ctx,args,0,f)):NULL;
    curr_ctx = ctx; 
    buf = readline(p);
    if(!buf) return naNil();
    naRef r = NASTR(buf);
    free(buf);
    return r;
}

static naRef f_set_completion_func(naContext ctx, naRef me, int argc, naRef* args)
{
    char *f = "set_completion_entry_function";
    check_argc(ctx,argc,1,f);
    completion_func = arg_func(ctx,args,0,f);
    naHash_set(keep_this,NASTR("completion_func"),completion_func);
    return naNil();
}

static naRef f_set_word_break_chars(naContext ctx, naRef me, int argc, naRef* args)
{
    char *f = "set_completer_word_break_characters";
    naRef r;
    check_argc(ctx,argc,1,f);
    r = arg_str(ctx,args,0,f);
    rl_completer_word_break_characters = naStr_data(r);
    naHash_set(keep_this,NASTR("word_break_chars"),r);
    return naNil();
}

static naRef f_set_append_char(naContext ctx, naRef me, int argc, naRef* args)
{
    char *f = "set_completion_append_character";
    int c;
    check_argc(ctx,argc,1,f);
    c = (int)arg_num(ctx,args,0,f);
    rl_completion_append_character = c;
    return naNil();
}

static naRef f_add_history(naContext ctx, naRef me, int argc, naRef* args)
{
    char *f = "add_history";
    naRef r;
    check_argc(ctx,argc,1,f);
    r = arg_str(ctx,args,0,f);
    add_history(naStr_data(r));
    return naNil();
}

static naCFuncItem funcs[] = {
    { "readline", f_readline },
    { "set_completion_entry_function", f_set_completion_func },
    { "set_completer_word_break_characters", f_set_word_break_chars },
    { "set_completion_append_character", f_set_append_char },
    { "add_history", f_add_history },
    { 0 }
};

naRef naInit_readline(naContext c)
{
    keep_this = naNewHash(c);
    naSave(c,keep_this);
    
    completion_func = naNil();
    rl_completion_entry_function = completion_wrapper;

    //defaults
    rl_completer_word_break_characters = " \t\n\"\\'`@$><=;,|&{}()[]+-/*";
    rl_completion_append_character = '\0';

    naRef ns = naGenLib(c, funcs);
    return ns;
}

