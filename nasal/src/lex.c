#include <stdio.h> // DEBUG
#include <stdlib.h> // DEBUG

#include "parse.h"

// Static table of recognized lexemes in the language
struct Lexeme {
    char* str;
    int   tok;
} LEXEMES[] = {
    {"!", TOK_NOT},
    {"(", TOK_LPAR},
    {")", TOK_RPAR},
    {"[", TOK_LBRA},
    {"]", TOK_RBRA},
    {"{", TOK_LCURL},
    {"}", TOK_RCURL},
    {"*", TOK_MUL},
    {"+", TOK_PLUS},
    {"-", TOK_MINUS},
    {"/", TOK_DIV},
    {"~", TOK_CAT},
    {":", TOK_COLON},
    {".", TOK_DOT},
    {",", TOK_COMMA},
    {";", TOK_SEMI},
    {"=", TOK_ASSIGN},
    {"<",  TOK_LT},
    {"<=", TOK_LTE},
    {"==", TOK_EQ},
    {"!=", TOK_NEQ},
    {">",  TOK_GT},
    {">=", TOK_GTE},
    {"if",    TOK_IF},
    {"elsif", TOK_ELSIF},
    {"else",  TOK_ELSE},
    {"for",     TOK_FOR},
    {"foreach", TOK_FOREACH},
    {"while",   TOK_WHILE},
    {"return",   TOK_RETURN},
    {"break",    TOK_BREAK},
    {"continue", TOK_CONTINUE},
    {"func", TOK_FUNC}
};

// Build a table of where each line ending is
static int* findLines(struct Parser* p)
{
    char* buf = p->buf;
    int sz = p->len/10;
    int* lines = naParseAlloc(p, (sizeof(int) * sz));
    int i, j, n=0;
    
    for(i=0; i<p->len; i++) {
        // Not a line ending at all
        if(buf[i] != '\n' && buf[i] != '\r')
            continue;
        
        // Skip over the \r of a \r\n pair.
        if(buf[i] == '\r' && (i+1)<p->len && buf[i+1] == '\n') {
            i++;
            continue;
        }
        
        // Reallocate if necessary
        if(n == sz) {
            sz *= 2;
            int* nl = naParseAlloc(p, sizeof(int) * sz);
            for(j=0; j<n; j++) nl[j] = lines[j];
            lines = nl;
        }
        
        lines[n++] = i;
    }
    p->lines = lines;
    p->nLines = n;
    return lines;
}

// What line number is the index on?
static int getLine(struct Parser* p, int index)
{
    int i;
    for(i=0; i<p->nLines; i++)
        if(p->lines[i] > index)
            return i+1;
    return p->nLines+1;
}

static void error(struct Parser* p, char* msg, int index)
{
    // FIXME: use a longjmp to a handler in the parser
    printf("Error: %s at line %d\n", msg, getLine(p, index));
    exit(1);
    
}

// End index (the newline character) of the given line
static int lineEnd(struct Parser* p, int line)
{
    if(line > p->nLines) return p->len;
    return p->lines[line-1];
}

char* tokStrDEBUG(int tok)
{
    switch(tok) {
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

// Forward reference to permit recursion with newToken
static void collectSymbol(struct Parser* p, int onePastEnd);

static void newToken(struct Parser* p, int pos, int type,
                     char* str, int slen, double num)
{
    struct Token* tok;
    
    // Push an accumulating symbol first
    if(p->symbolStart >= 0)
        collectSymbol(p, pos);
    
    tok = naParseAlloc(p, sizeof(struct Token));
    tok->type = type;
    tok->line = getLine(p, pos);
    tok->str = str;
    tok->strlen = slen;
    tok->num = num;
    tok->next = 0;
    tok->prev = p->tail;
    tok->children = 0;
    
    if(p->tail) p->tail->next = tok;
    p->tail = tok;

    // DEBUG
    {
        int i;
        printf("line %d %s ", getLine(p, pos), tokStrDEBUG(type));
        if(str) {
            printf("\"");
            for(i=0; i<slen; i++) printf("%c", str[i]);
            printf("\" ");
        }
        printf("(%f)\n", num);
    }
    
}

// Emits a symbol that has been accumulating in the lexer
static void collectSymbol(struct Parser* p, int onePastEnd)
{
    int start = p->symbolStart;
    p->symbolStart = -1;
    newToken(p, start, TOK_SYMBOL, p->buf + start, onePastEnd - start, 0);
}

// Parse a hex nibble
static int hexc(char c, struct Parser* p, int index)
{
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'A' && c <= 'F') return c - 'a' + 10;
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    error(p, "Bad hex constant", index);
    return 0;
}

// Escape and returns a single backslashed expression in a single
// quoted string.  Trivial, just escape \' and leave everything else
// alone.
static void sqEscape(char* buf, int len, int index, struct Parser* p,
                     char* cOut, int* eatenOut)
{
    if(len < 2) error(p, "Unterminated string", index);
    if(buf[1] == '\'') {
        *cOut = '\'';
        *eatenOut = 2;
    } else {
        *cOut = '\\';
        *eatenOut = 1;
    }
}

// Ditto, but more complicated for double quotes.
static void dqEscape(char* buf, int len, int index, struct Parser* p,
                     char* cOut, int* eatenOut)
{
    if(len < 2) error(p, "Unterminated string", index);
    *eatenOut = 2;
    switch(buf[1]) {
    case '"': *cOut = '"'; break; 
    case 'r': *cOut = '\r'; break; 
    case 'n': *cOut = '\n'; break; 
    case 't': *cOut = '\t'; break; 
    case 'x':
        if(len < 4) error(p, "Unterminated string", index);
        *cOut = (char)((hexc(buf[2], p, index)<<4) | hexc(buf[3], p, index));
        *eatenOut = 4;
    default:
        // Unhandled, put the backslash back
        *cOut = '\\';
        *eatenOut = 1;
    }
}

// Read in a string literal
static int lexStringLiteral(struct Parser* p, int index, int singleQuote)
{
    int i, j=0, len=0, iteration;
    char* out = 0;
    char* buf = p->buf;
    char endMark = singleQuote ? '\'' : '"';

    for(iteration = 0; iteration<2; iteration++) {
        i = index+1;
        while(i < p->len) {
            char c = buf[i];
            int eaten = 1;
            if(c == endMark)
                break;
            if(c == '\\') {
                if(singleQuote) sqEscape(buf+i, len-i, i, p, &c, &eaten);
                else            dqEscape(buf+i, len-i, i, p, &c, &eaten);
            }
            if(iteration == 1) out[j++] = c;
            i += eaten;
            len++;
        }

        // Finished stage one -- allocate the buffer for stage two
        if(iteration == 0)
            out = naParseAlloc(p, len);
    }

    newToken(p, index, TOK_LITERAL, out, len, 0);

    return i;
}

static int lexNumLiteral(struct Parser* p, int index)
{
    int len = p->len, i = index;
    unsigned char* buf = p->buf;
    double d;

    while(i<len && buf[i] >= '0' && buf[i] <= '9') i++;
    if(i<len && buf[i] == '.') {
        i++;
        while(i<len && buf[i] >= '0' && buf[i] <= '9') i++;
        if(i<len && (buf[i] == 'e' || buf[i] == 'E')) {
            i++;
            if(i<len
               && (buf[i] == '-' || buf[i] == '+')
               && (i+1<len && buf[i+1] >= '0' && buf[i+1] <= '9')) i++;
            while(i<len && buf[i] >= '0' && buf[i] <= '9') i++;
        }
    }

    naStr_parsenum(p->buf + index, i - index, &d);
    newToken(p, index, TOK_LITERAL, 0, 0, d);
    return i;
}

// Returns the length of lexeme l if the buffer prefix matches, or
// else zero.
static int matchLexeme(char* buf, int len, char* l)
{
    int i;
    for(i=0; i<len; i++) {
        if(l[i] == 0)      return i;
        if(l[i] != buf[i]) return 0;
    }
    // Ran out of buffer.  This is still OK if we're also at the end
    // of the lexeme.
    if(l[i] == 0) return i;
    return 0;
}

// This is dumb and algorithmically slow.  It would be much more
// elegant to sort and binary search the lexeme list, but that's a lot
// more code and this really isn't very slow in practice; it checks
// every byte of every lexeme for each input byte.  There are less
// than 100 bytes of lexemes in the grammar.  Returns the number of
// bytes in the lexeme read (or zero if none was recognized)
static int tryLexemes(struct Parser* p, int index)
{
    int i, n, best, bestIndex=-1;
    char* start = p->buf + index;
    int len = p->len - index;

    n = sizeof(LEXEMES) / sizeof(struct Lexeme);
    best = 0;
    for(i=0; i<n; i++) {
        int l = matchLexeme(start, len, LEXEMES[i].str);
        if(l > best) {
            best = l;
            bestIndex = i;
        }
    }
    if(best > 0)
        newToken(p, index, LEXEMES[bestIndex].tok, 0, 0, 0);
    return best;
}

void naLex(struct Parser* p)
{
    int i=0;

    findLines(p);

    for(i=0; i<p->len; i++) {
        int handled = 1;
        switch(p->buf[i]) {
        case ' ': case '\t': case '\n': case '\r': case '\f': case '\v':
            break; // ignore whitespace
        case '#':
            i = lineEnd(p, getLine(p, i));
            break; // comment to EOL
        case '\'':
            i = lexStringLiteral(p, i, 1);
            break;
        case '"':
            i = lexStringLiteral(p, i, 0);
            break;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            if(p->symbolStart >= 0) {
                // Ignore numbers when parsing a symbol
                handled = 0;
                break;
            }
            i = lexNumLiteral(p, i);
            break;
        default:
            handled = 0;
        }

        if(handled && p->symbolStart >= 0)
            collectSymbol(p, i);

        if(!handled) {
            int lexlen = tryLexemes(p, i);

            // Got nothing?  Then it's a symbol char.  Is it the first?
            if(lexlen == 0 && p->symbolStart < 0)
                p->symbolStart = i;

            if(lexlen > 1)
                i += lexlen - 1;
        }
    }
}