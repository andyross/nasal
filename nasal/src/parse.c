#include <stdio.h> // DEBUG
#include <stdlib.h> // DEBUG

#include "parse.h"

// Grammar FIXME's:
//
// Need to insert semi's after blocks.

// Static precedence table, from low (loose binding, do first) to high
// (tight binding, do last).
enum { PREC_BINARY, PREC_REVERSE, PREC_PREFIX, PREC_SUFFIX };

#define MAX_PREC_TOKS 5
struct precedence {
    int toks[MAX_PREC_TOKS];
    int rule;
} PRECEDENCE[] = {
    { { TOK_SEMI, TOK_COMMA },                 PREC_REVERSE },
    { { TOK_RETURN, TOK_BREAK, TOK_CONTINUE }, PREC_PREFIX  },
    { { TOK_ASSIGN },                          PREC_REVERSE },
    { { TOK_OR },                              PREC_BINARY  },
    { { TOK_AND },                             PREC_BINARY  },
    { { TOK_EQ, TOK_NEQ },                     PREC_BINARY  },
    { { TOK_LT, TOK_LTE, TOK_GT, TOK_GTE },    PREC_BINARY  },
    { { TOK_PLUS, TOK_MINUS, TOK_CAT },        PREC_BINARY  },
    { { TOK_MUL, TOK_DIV },                    PREC_BINARY  },
    { { TOK_MINUS, TOK_NOT },                  PREC_PREFIX  },
    { { TOK_DOT },                             PREC_BINARY  },
    { { TOK_LPAR, TOK_LBRA },                  PREC_SUFFIX  },
};
#define PRECEDENCE_LEVELS (sizeof(PRECEDENCE)/sizeof(struct precedence))

// FIXME - longjmp
static void error(char* msg, int line)
{
    printf("Error: %s at line %d\n", msg, line);
    exit(1);
}

// Generic parse error
static void oops(struct Token* t)
{
    error("parse error", t->line);
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

    p->tree.type = TOK_TOP;
    p->tree.line = 1;
    p->tree.str = 0;
    p->tree.strlen = 0;
    p->tree.num = 0;
    p->tree.next = 0;
    p->tree.prev = 0;
    p->tree.children = 0;
    p->tree.lastChild = 0;
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

// Remove the child from the list where it exists, and insert it at
// the end of the parents child list.
static void addNewChild(struct Token* p, struct Token* c)
{
    if(c->prev) c->prev->next = c->next;

    if(c->next) c->next->prev = c->prev;

    c->next = 0;
    c->prev = p->lastChild;
    if(p->lastChild) p->lastChild->next = c;
    if(!p->children) p->children = c;
    p->lastChild = c;
}

// Follows the token list from start (which must be a left brace of
// some type), placing all tokens found into start's child list until
// it reaches the matching close brace.
static void collectBrace(struct Token* start)
{
    struct Token* t;
    int closer = -1;
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
        // Snip t out of the existing list, and append it to start's
        // children.
        next = t->next;
        addNewChild(start, t);
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

// Fixes up parenting for obvious parsing situations, like code blocks
// being the child of a func keyword, etc...
static void fixBlockStructure(struct Token* start)
{
    struct Token *t, *c;
    t = start;
    while(t) {
        switch(t->type) {
        case TOK_ELSE: case TOK_FUNC:
            // These guys precede a single curly block
            if(!t->next || t->next->type != TOK_LCURL) oops(t);
            c = t->next;
            addNewChild(t, c);
            fixBlockStructure(c);
            break;
        case TOK_FOR: case TOK_FOREACH: case TOK_WHILE:
        case TOK_IF: case TOK_ELSIF:
            // Expect a paren and then a curly
            if(!t->next || t->next->type != TOK_LPAR) oops(t);
            c = t->next;
            addNewChild(t, c);
            fixBlockStructure(c);

            if(!t->next || t->next->type != TOK_LCURL) oops(t);
            c = t->next;
            addNewChild(t, c);
            fixBlockStructure(c);
            break;
        case TOK_LPAR: case TOK_LBRA: case TOK_LCURL:
            fixBlockStructure(t->children);
            break;
        }
        t = t->next;
    }

    // Another pass to hook up the elsif/else chains
    t = start;
    while(t) {
        if(t->type == TOK_IF) {
            while(t->next && t->next->type == TOK_ELSIF)
                addNewChild(t, t->next);
            if(t->next && t->next->type == TOK_ELSE)
                addNewChild(t, t->next);
        }
        t = t->next;
    }
}

// True if the token's type exists in the precedence level.
static int tokInLevel(struct Token* tok, int level)
{
    int i;
    for(i=0; i<MAX_PREC_TOKS; i++)
        if(PRECEDENCE[level].toks[i] == tok->type)
            return 1;
    return 0;
}

// Allocate and return an "empty" token as a parsing placeholder.
static struct Token* emptyToken(struct Parser* p)
{
    struct Token* t = naParseAlloc(p, sizeof(struct Token));
    t->type = TOK_EMPTY;
    t->line = 0;
    t->str = 0;
    t->strlen = 0;
    t->num = 0;
    t->next = 0;
    t->prev = 0;
    t->children = 0;
    t->lastChild = 0;
    return t;
}

static struct Token* parsePrecedence(struct Parser* p, struct Token* start,
                                     struct Token* end, int level)
{
    int rule;
    struct Token *t, *top, *left, *right;

    // This is an error.  No "siblings" are allowed at the bottom level.
    if(level >= PRECEDENCE_LEVELS && start != end)
        oops(start);

    // Synthesize an empty token if necessary
    if(end == 0 && start == 0)
        return emptyToken(p);

    // Sanify the list.  This is OK, since we're recursing into the
    // list structure; stuff to the left and right has already been
    // handled somewhere above.
    if(end == 0) end = start;
    if(start == 0) start = end;
    start->prev = end->next = 0;

    // Single tokens parse as themselves.  Remember to parse any
    // preexisting (block structure) children.
    if(start == end) {
        if(start->children) {
            t = start->children;
            while(t) {
                if(t->type == TOK_LPAR
                   || t->type == TOK_LBRA
                   || t->type == TOK_LCURL)
                {
                    top = parsePrecedence(p, t->children, t->lastChild, 0);
                    t->children = top;
                    t->lastChild = top;
                }
                t = t->next;
            }
        }
        return start;
    }

    top = left = right = 0;
    rule = PRECEDENCE[level].rule;
    switch(rule) {
    case PREC_PREFIX:
        if(tokInLevel(start, level) && start->next) {
            left = parsePrecedence(p, start->children, start->lastChild, 0);
            top = start;
            right = parsePrecedence(p, start->next, end, level);
        }
        break;
    case PREC_SUFFIX:
        if(tokInLevel(end, level) && end->prev) {
            left = parsePrecedence(p, start, end->prev, level);
            top = end;
            right = parsePrecedence(p, end->children, end->lastChild, 0);
        }
        break;
    case PREC_BINARY:
        t = end;
        while(t) {
            if(tokInLevel(t, level)) {
                left = parsePrecedence(p, t->prev?start:0, t->prev, level);
                top = t;
                right = parsePrecedence(p, t->next, t->next?end:0, level+1);
                break;
            }
            t = t->prev;
        }
        break;
    case PREC_REVERSE:
        t = start;
        while(t) {
            if(tokInLevel(t, level)) {
                left = parsePrecedence(p, t->prev?start:0, t->prev, level+1);
                top = t;
                right = parsePrecedence(p, t->next, t->next?end:0, level);
                break;
            }
            t = t->next;
        }
        break;
    }

    // Found nothing, try the next level
    if(!top)
        return parsePrecedence(p, start, end, level+1);

    left->next = right;
    left->prev = 0;
    right->next = 0;
    right->prev = left;
    top->children = left;
    top->lastChild = right;
    return top;
}

void dumpTokenList(struct Token* t, int prefix); // DEBUG

void naParse(struct Parser* p)
{
    struct Token* t;

    naLex(p);
    braceMatch(p->tree.children);
    fixBlockStructure(p->tree.children);

    dumpTokenList(&p->tree, 0); // DEBUG

    t = parsePrecedence(p, p->tree.children, p->tree.lastChild, 0);
    t->prev = t->next = 0;
    p->tree.children = t;
    p->tree.lastChild = t;

    printf("\n\n"); // DEBUG
}
