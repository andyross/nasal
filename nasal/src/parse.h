#ifndef _PARSE_H
#define _PARSE_H

#include "nasl.h"

enum {
    TOK_NOT, TOK_LPAR, TOK_RPAR, TOK_LBRA, TOK_RBRA, TOK_LCURL,
    TOK_RCURL, TOK_MUL, TOK_PLUS, TOK_MINUS, TOK_DIV, TOK_CAT,
    TOK_COLON, TOK_DOT, TOK_COMMA, TOK_SEMI, TOK_ASSIGN, TOK_LT,
    TOK_LTE, TOK_EQ, TOK_NEQ, TOK_GT, TOK_GTE, TOK_IF, TOK_ELSIF,
    TOK_ELSE, TOK_FOR, TOK_FOREACH, TOK_WHILE, TOK_RETURN,
    TOK_BREAK, TOK_CONTINUE, TOK_FUNC, TOK_SYMBOL, TOK_LITERAL 
};

struct Token {
    int type;
    int line;
    char* str;
    int strlen;
    double num;
    struct Token* next;
    struct Token* prev;
    struct Token* children;
    struct Token* lastChild;
};

struct Parser {
    // The parse tree
    struct Token* tree;

    // The input buffer
    char* buf;
    int   len;

    // Computed line number table for the lexer
    int* lines;
    int  nLines;

    // Chunk allocator.  Throw away after parsing.
    void** chunks;
    int* chunkSizes;
    int nChunks;
    int leftInChunk;

    // Start of current symbol in the lexer
    int symbolStart;
    
    // End of the lexer token list
    struct Token* tail;
};

void naParseInit(struct Parser* p);
void* naParseAlloc(struct Parser* p, int bytes);
void naParseDestroy(struct Parser* p);

void naLex(struct Parser* p);
void naParse(struct Parser* p);

#endif // _PARSE_H
