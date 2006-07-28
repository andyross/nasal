#include <stdio.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "nasal.h"

static naRef f_readline(naContext c, naRef me, int argc, naRef* args)
{
    naRef prompt = argc > 0 ? naStringValue(c, args[0]) : naNil();
    char* p = naIsNil(prompt) ? "> " : naStr_data(prompt);
    char* line = readline(p);
    if(line) add_history(line);
    return line ? naStr_fromdata(naNewString(c), line, strlen(line)) : naNil();
}

static naCFuncItem funcs[] = {
    { "readline", f_readline },
    { 0 }
};

naRef naInit_readline(naContext c)
{
    return naGenLib(c, funcs);
}
