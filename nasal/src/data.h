#ifndef _DATA_H
#define _DATA_H

#include "nasal.h"

// Notes: A CODE object is a compiled set of bytecode instructions.
// What actually gets executed at runtime is a bound FUNC object,
// which combines the raw code with a namespace and a pointer to
// parent function in the lexical closure.
enum { T_STR, T_VEC, T_HASH, T_CODE, T_FUNC, T_CCODE, T_GHOST,
       NUM_NASAL_TYPES }; // V. important that this come last!

#define IS_REF(r) ((r).ref.reftag == NASAL_REFTAG)
#define IS_NUM(r) ((r).ref.reftag != NASAL_REFTAG)
#define IS_OBJ(r) (IS_REF((r)) && (r).ref.ptr.obj != 0)
#define IS_NIL(r) (IS_REF((r)) && (r).ref.ptr.obj == 0)
#define IS_STR(r) (IS_OBJ((r)) && (r).ref.ptr.obj->type == T_STR)
#define IS_VEC(r) (IS_OBJ((r)) && (r).ref.ptr.obj->type == T_VEC)
#define IS_HASH(r) (IS_OBJ((r)) && (r).ref.ptr.obj->type == T_HASH)
#define IS_CODE(r) (IS_OBJ((r)) && (r).ref.ptr.obj->type == T_CODE)
#define IS_FUNC(r) (IS_OBJ((r)) && (r).ref.ptr.obj->type == T_FUNC)
#define IS_CCODE(r) (IS_OBJ((r)) && (r).ref.ptr.obj->type == T_CCODE)
#define IS_GHOST(r) (IS_OBJ((r)) && (r).ref.ptr.obj->type == T_GHOST)
#define IS_CONTAINER(r) (IS_VEC(r)||IS_HASH(r))
#define IS_SCALAR(r) (IS_NUM((r)) || IS_STR((r)))
#define IDENTICAL(a, b) (IS_REF(a) && IS_REF(b) \
                         && a.ref.ptr.obj == b.ref.ptr.obj)

// This is a macro instead of a separate struct to allow compilers to
// avoid padding.  GCC on x86, at least, will always padd the size of
// an embedded struct up to 32 bits.  Doing it this way allows the
// implementing objects to pack in 16 bits worth of data "for free".
#define GC_HEADER \
    unsigned char mark; \
    unsigned char type

struct naObj {
    GC_HEADER;
};

struct naStr {
    GC_HEADER;
    int len;
    unsigned char* data;
    unsigned int hashcode;
};

struct naVec {
    GC_HEADER;
    int size;
    int alloced;
    naRef* array;
};

struct HashNode {
    naRef key;
    naRef val;
    struct HashNode* next;
};

struct naHash {
    GC_HEADER;
    int size;
    int lgalloced;
    struct HashNode* nodes;
    struct HashNode** table;
    int nextnode;
};

struct naCode {
    GC_HEADER;
    unsigned char nArgs;
    unsigned char nOptArgs;
    unsigned char needArgVector;
    unsigned short nConstants;
    unsigned short nBytes;
    unsigned short nLines;
    unsigned char* byteCode;
    naRef* constants;
    int* argSyms; // indices into constants
    int* optArgSyms;
    int* optArgVals;
    unsigned short* lineIps; // pairs of {ip, line}
    naRef srcFile;
    naRef restArgSym; // The "..." vector name, defaults to "arg"
};

struct naFunc {
    GC_HEADER;
    naRef code;
    naRef namespace;
    naRef next; // parent closure
};

struct naCCode {
    GC_HEADER;
    naCFunction fptr;
};

struct naGhost {
    GC_HEADER;
    naGhostType* gtype;
    void* ptr;
};

struct naPool {
    int           type;
    int           elemsz;
    int           nblocks;
    struct Block* blocks;
    int           nfree;   // number of entries in the free array
    int           freesz;  // size of the free array
    void**        free;    // pointers to usable elements
};

void naFree(void* m);
void* naAlloc(int n);
void naBZero(void* m, int n);

int naTypeSize(int type);
void naGarbageCollect();
naRef naObj(int type, struct naObj* o);
naRef naNew(naContext c, int type);
naRef naNewCode(naContext c);

int naStr_equal(naRef s1, naRef s2);
naRef naStr_fromnum(naRef dest, double num);
int naStr_numeric(naRef str);
int naStr_parsenum(char* str, int len, double* result);
int naStr_tonum(naRef str, double* out);

void naVec_init(naRef vec);

int naHash_tryset(naRef hash, naRef key, naRef val); // sets if exists
int naHash_sym(struct naHash* h, struct naStr* sym, naRef* out);
void naHash_newsym(struct naHash* h, naRef* sym, naRef* val);
void naHash_init(naRef hash);

void naGC_init(struct naPool* p, int type);
struct naObj* naGC_get(struct naPool* p);
int naGC_size(struct naPool* p);
void naGC_mark(naRef r);
void naGC_reap(struct naPool* p);

void naStr_gcclean(struct naStr* s);
void naVec_gcclean(struct naVec* s);
void naHash_gcclean(struct naHash* s);

#endif // _DATA_H
