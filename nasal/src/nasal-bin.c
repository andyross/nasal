#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "parse.h"

char* tokString(int tok)
{
    switch(tok) {
    case TOK_EMPTY: return "TOK_EMPTY";
    case TOK_NOT: return "TOK_NOT";
    case TOK_LPAR: return "TOK_LPAR";
    case TOK_RPAR: return "TOK_RPAR";
    case TOK_LBRA: return "TOK_LBRA";
    case TOK_RBRA: return "TOK_RBRA";
    case TOK_LCURL: return "TOK_LCURL";
    case TOK_RCURL: return "TOK_RCURL";
    case TOK_MUL: return "TOK_MUL";
    case TOK_PLUS: return "TOK_PLUS";
    case TOK_MINUS: return "TOK_MINUS";
    case TOK_DIV: return "TOK_DIV";
    case TOK_CAT: return "TOK_CAT";
    case TOK_COLON: return "TOK_COLON";
    case TOK_DOT: return "TOK_DOT";
    case TOK_COMMA: return "TOK_COMMA";
    case TOK_SEMI: return "TOK_SEMI";
    case TOK_ASSIGN: return "TOK_ASSIGN";
    case TOK_LT: return "TOK_LT";
    case TOK_LTE: return "TOK_LTE";
    case TOK_EQ: return "TOK_EQ";
    case TOK_NEQ: return "TOK_NEQ";
    case TOK_GT: return "TOK_GT";
    case TOK_GTE: return "TOK_GTE";
    case TOK_IF: return "TOK_IF";
    case TOK_ELSIF: return "TOK_ELSIF";
    case TOK_ELSE: return "TOK_ELSE";
    case TOK_FOR: return "TOK_FOR";
    case TOK_FOREACH: return "TOK_FOREACH";
    case TOK_WHILE: return "TOK_WHILE";
    case TOK_RETURN: return "TOK_RETURN";
    case TOK_BREAK: return "TOK_BREAK";
    case TOK_CONTINUE: return "TOK_CONTINUE";
    case TOK_FUNC: return "TOK_FUNC";
    case TOK_SYMBOL: return "TOK_SYMBOL";
    case TOK_LITERAL: return "TOK_LITERAL";
    }
    return 0;
}

void printToken(struct Token* t, char* prefix)
{
    int i;
    printf("%sline %d %s ", prefix, t->line, tokString(t->type));
    if(t->type == TOK_LITERAL || t->type == TOK_SYMBOL) {
        if(t->str) {
            printf("\"");
            for(i=0; i<t->strlen; i++) printf("%c", t->str[i]);
            printf("\" (len: %d)", t->strlen);
        } else {
            printf("%f ", t->num);
        }
    }
    printf("\n");
}

void dumpTokenList(struct Token* t, int prefix)
{
    char prefstr[128];
    int i;

    prefstr[0] = 0;
    for(i=0; i<prefix; i++)
        strcat(prefstr, ". ");

    while(t) {
        printToken(t, prefstr);
        dumpTokenList(t->children, prefix+1);
        t = t->next;
    }
}

int main(int argc, char** argv)
{
    int i;
    FILE* f;
    struct stat fdat;
    char* buf;
    struct Parser p;

    for(i=1; i<argc; i++) {
        stat(argv[i], &fdat);
        buf = malloc(fdat.st_size);
        f = fopen(argv[i], "r");
        fread(buf, 1, fdat.st_size, f);

        naParseInit(&p);

        p.buf = buf;
        p.len = fdat.st_size;

        naParse(&p);

        dumpTokenList(p.tree.children, 0);

        naParseDestroy(&p);
    }
    return 0;
}
