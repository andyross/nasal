#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "parse.h"

// Bytecode operator to string
char* opStringDEBUG(int op)
{
    switch(op) {
    case OP_AND: return "AND";
    case OP_OR: return "OR";
    case OP_NOT: return "NOT";
    case OP_MUL: return "MUL";
    case OP_PLUS: return "PLUS";
    case OP_MINUS: return "MINUS";
    case OP_DIV: return "DIV";
    case OP_NEG: return "NEG";
    case OP_CAT: return "CAT";
    case OP_LT: return "LT";
    case OP_LTE: return "LTE";
    case OP_GT: return "GT";
    case OP_GTE: return "GTE";
    case OP_EQ: return "EQ";
    case OP_NEQ: return "NEQ";
    case OP_EACH: return "EACH";
    case OP_JMP: return "JMP";
    case OP_JIFNOT: return "JIFNOT";
    case OP_JIFNIL: return "JIFNIL";
    case OP_FCALL: return "FCALL";
    case OP_MCALL: return "MCALL";
    case OP_RETURN: return "RETURN";
    case OP_PUSHCONST: return "PUSHCONST";
    case OP_PUSHONE: return "PUSHONE";
    case OP_PUSHZERO: return "PUSHZERO";
    case OP_PUSHNIL: return "PUSHNIL";
    case OP_POP: return "POP";
    case OP_DUP: return "DUP";
    case OP_XCHG: return "XCHG";
    case OP_INSERT: return "INSERT";
    case OP_EXTRACT: return "EXTRACT";
    case OP_MEMBER: return "MEMBER";
    case OP_SETMEMBER: return "SETMEMBER";
    case OP_LOCAL: return "LOCAL";
    case OP_SETLOCAL: return "SETLOCAL";
    case OP_NEWVEC: return "NEWVEC";
    case OP_VAPPEND: return "VAPPEND";
    case OP_NEWHASH: return "NEWHASH";
    case OP_HAPPEND: return "HAPPEND";
    case OP_LINE: return "LINE";
    case OP_MARK: return "MARK";
    case OP_UNMARK: return "UNMARK";
    case OP_BREAK: return "BREAK";
    }
    return "<bad opcode>";
}

// Print a bytecode operator
void printOpDEBUG(int ip, int op)
{
    printf("IP: %d OP: %s\n", ip, opStringDEBUG(op));
}

// Print a naRef
void printRefDEBUG(naRef r)
{
    int i;
    if(IS_NUM(r)) {
        printf("%f\n", r.num);
    } else if(IS_NIL(r)) {
        printf("<nil>\n");
    } else if(IS_STR(r)) {
        printf("\"");
        for(i=0; i<r.ref.ptr.str->len; i++)
            printf("%c", r.ref.ptr.str->data[i]);
        printf("\"\n");
    } else if(IS_VEC(r)) {
        printf("<vec>\n");
    } else if(IS_HASH(r)) {
        printf("<hash>\n");
    } else if(IS_FUNC(r)) {
        printf("<func>\n");
    } else if(IS_CLOSURE(r)) {
        printf("DEBUG: closure object on stack!\n");
    } else if(IS_CODE(r)) {
        printf("DEBUG: code object on stack!\n");
    } else printf("DEBUG ACK\n");
}

// Print the operand stack of the specified context
void printStackDEBUG(struct Context* ctx)
{
    int i;
    printf("\n");
    for(i=ctx->opTop-1; i>=0; i--) {
        printf("] ");
        printRefDEBUG(ctx->opStack[i]);
    }
    printf("\n");
}

// Token type to string
char* tokString(int tok)
{
    switch(tok) {
    case TOK_TOP: return "TOK_TOP";
    case TOK_AND: return "TOK_AND";
    case TOK_OR: return "TOK_OR";
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
    case TOK_EMPTY: return "TOK_EMPTY";
    case TOK_NIL: return "TOK_NIL";
    }
    return 0;
}

// Diagnostic: check all list pointers for sanity
void ack()
{
    printf("ACK!");
    exit(1);
}
void checkList(struct Token* start, struct Token* end)
{
    struct Token* t = start;
    while(t) {
        if(t->next && t->next->prev != t) ack();
        if(t->next==0 && t != end) ack();
        t = t->next;
    }
    t = end;
    while(t) {
        if(t->prev && t->prev->next != t) ack();
        if(t->prev==0 && t != start) ack();
        t = t->prev;
    };
}


// Prints a single parser token to stdout
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

// Prints a parse tree to stdout
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

static naRef print(naContext c, naRef args)
{
    int i, n;
    n = naVec_size(args);
    for(i=0; i<n; i++) {
        naRef s = naStringValue(c, naVec_get(args, i));
        if(IS_NIL(s)) continue;
        fwrite(naStr_data(s), 1, naStr_len(s), stdout);
    }
    return naNil();
}

int main(int argc, char** argv)
{
    FILE* f;
    struct stat fdat;
    char* buf;
    struct Context *ctx;
    naRef code, namespace, result, name;

    if(argc < 2) {
        fprintf(stderr, "nasl: must specify a script to run\n");
        exit(1);
    }

    // Read the contents of the file into a buffer in memory
    stat(argv[1], &fdat);
    buf = malloc(fdat.st_size);
    f = fopen(argv[1], "r");
    if(!f) {
        fprintf(stderr, "nasl: could not open input file: %s\n", argv[1]);
        exit(1);
    }
    if(fread(buf, 1, fdat.st_size, f) != fdat.st_size) {
        fprintf(stderr, "nasl: error in fread()\n");
        exit(1);
    }
    
    // Create an interpreter context
    ctx = naNewContext();

    // Parse the code in the buffer
    code = naParseCode(ctx, buf, fdat.st_size);

    // Make a hash containing the standard library functions.  This
    // will be the namespace for a new script (more elaborate
    // environments -- imported libraries, for example -- might be
    // different).
    namespace = naStdLib(ctx);

    // Add application-specific functions (in this case, "print") to
    // the namespace if desired.
    name = naNewString(ctx);
    naStr_fromdata(name, "print", 5);
    naHash_set(namespace,
               name,
               naNewFunc(ctx,
                         naNewCCode(ctx, print), // CCODE object
                         naNil())); // function closure (none here)

    // Run it
    result = naRun(ctx, code, namespace);
    
    free(buf);
    return 0;
}
