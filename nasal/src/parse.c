#include <stdio.h> // DEBUG
#include <stdlib.h> // DEBUG

#include "parse.h"

// FIXME
static void error(char* msg, int line)
{
    printf("Error: %s at line %d\n", msg, line);
    exit(1);
}

void naParseInit(struct Parser* p)
{
    p->buf = 0;
    p->len = 0;
    p->lines = 0;
    p->nLines = 0;
    p->chunks = 0;
    p->chunkSizes = 0;
    p->nChunks = 0;
    p->leftInChunk = 0;
    p->symbolStart = -1;
    p->tree = 0;
    p->tail = 0;
}

void naParseDestroy(struct Parser* p)
{
    int i;
    for(i=0; i<p->nChunks; i++) FREE(p->chunks[i]);
    FREE(p->chunks);
    FREE(p->chunkSizes);
}

void* naParseAlloc(struct Parser* p, int bytes)
{
    // Round up to 8 byte chunks for alignment
    if(bytes & 0x7) bytes = ((bytes>>3) + 1) << 3;
    
    // Need a new chunk?
    if(p->leftInChunk < bytes) {
        void* newChunk;
        void** newChunks;
        int* newChunkSizes;
        int sz, i;

        sz = p->len;
        if(sz < bytes) sz = bytes;
        newChunk = ALLOC(sz);

        p->nChunks++;

        newChunks = ALLOC(p->nChunks * sizeof(void*));
        for(i=1; i<p->nChunks; i++) newChunks[i] = p->chunks[i-1];
        newChunks[0] = newChunk;
        FREE(p->chunks);
        p->chunks = newChunks;

        newChunkSizes = ALLOC(p->nChunks * sizeof(int));
        for(i=1; i<p->nChunks; i++) newChunkSizes[i] = p->chunkSizes[i-1];
        newChunkSizes[0] = sz;
        FREE(p->chunkSizes);
        p->chunkSizes = newChunkSizes;

        p->leftInChunk = sz;
    }

    char* result = p->chunks[0] + p->chunkSizes[0] - p->leftInChunk;
    p->leftInChunk -= bytes;
    return (void*)result;
}

// Follows the token list from start (which must be a left brace of
// some type), placing all tokens found into start's child list until
// it reaches the matching close brace.
static void collectBrace(struct Token* start)
{
    struct Token* t;
    int closer = -1;
    printf("collectBrace()...\n");
    if(start->type == TOK_LPAR)  closer = TOK_RPAR;
    if(start->type == TOK_LBRA)  closer = TOK_RBRA;
    if(start->type == TOK_LCURL) closer = TOK_RCURL;

    t = start->next;
    while(t) {
        struct Token* next;
        switch(t->type) {
        case TOK_LPAR: case TOK_LBRA: case TOK_LCURL:
            collectBrace(t);
            break;
        case TOK_RPAR: case TOK_RBRA: case TOK_RCURL:
            if(t->type != closer)
                error("mismatched closing brace", t->line);

            // Drop this node on the floor, stitch up the list and return
            start->next = t->next;
            if(t->next) t->next->prev = start;
            return;
        }
        next = t->next;

        // Snip t out of the existing list, and append it to start's
        // children.
        if(t->prev) t->prev->next = t->next;
        if(t->next) t->next->prev = t->prev;
        t->next = 0;
        t->prev = start->lastChild;
        if(start->lastChild) start->lastChild->next = t;
        if(!start->children) start->children = t;
        start->lastChild = t;

        t = next;
    }
    error("unterminated brace", start->line);
}

// Recursively find the contents of all matching brace pairs in the
// token list and turn them into children of the left token.  The
// right token disappears.
static void braceMatch(struct Token* start)
{
    struct Token* t = start;
    while(t) {
        switch(t->type) {
        case TOK_LPAR: case TOK_LBRA: case TOK_LCURL:
            collectBrace(t);
            break;
        case TOK_RPAR: case TOK_RBRA: case TOK_RCURL:
            if(start->type != TOK_LBRA)
                error("stray closing brace", t->line);
            break;
        }
        t = t->next;
    }
}

void naParse(struct Parser* p)
{
    naLex(p);
    braceMatch(p->tree);
}
