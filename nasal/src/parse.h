#ifndef _PARSE_H
#define _PARSE_H

#include <setjmp.h>

#include "nasl.h"
#include "code.h"

enum {
    TOK_TOP=1, TOK_AND, TOK_OR, TOK_NOT, TOK_LPAR, TOK_RPAR, TOK_LBRA,
    TOK_RBRA, TOK_LCURL, TOK_RCURL, TOK_MUL, TOK_PLUS, TOK_MINUS,
    TOK_DIV, TOK_CAT, TOK_COLON, TOK_DOT, TOK_COMMA, TOK_SEMI,
    TOK_ASSIGN, TOK_LT, TOK_LTE, TOK_EQ, TOK_NEQ, TOK_GT, TOK_GTE,
    TOK_IF, TOK_ELSIF, TOK_ELSE, TOK_FOR, TOK_FOREACH, TOK_WHILE,
    TOK_RETURN, TOK_BREAK, TOK_CONTINUE, TOK_FUNC, TOK_SYMBOL,
    TOK_LITERAL, TOK_EMPTY
};

struct Token {
    int type;
    int line;
    char* str;
    int strlen;
    double num;
    struct Token* parent;
    struct Token* next;
    struct Token* prev;
    struct Token* children;
    struct Token* lastChild;
};

struct Parser {
    // Handle to the NaSL interpreter
    struct Context* context;

    char* err;
    int errLine;
    jmp_buf jumpHandle;

    // The parse tree ubernode
    struct Token tree;

    // The input buffer
    char* buf;
    int   len;

    // Chunk allocator.  Throw away after parsing.
    void** chunks;
    int* chunkSizes;
    int nChunks;
    int leftInChunk;

    // Computed line number table for the lexer
    int* lines;
    int  nLines;

    // Accumulated byte code array
    unsigned char* byteCode;
    int nBytes;
    int codeAlloced;

    // Dynamic storage for constants, to be compiled into a static table
    naRef consts;   // index -> naRef
    naRef interned; // naRef -> index (scalars only!)
    int nConsts;
};

void naParseError(struct Parser* p, char* msg, int line);
void naParseInit(struct Parser* p);
void* naParseAlloc(struct Parser* p, int bytes);
void naParseDestroy(struct Parser* p);
void naLex(struct Parser* p);
naRef naCodeGen(struct Parser* p, struct Token* tok);

void naParse(struct Parser* p);



#endif // _PARSE_H
