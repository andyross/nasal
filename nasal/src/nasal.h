#ifndef _NASL_H
#define _NASL_H

// This is a nasl "reference".  They are always copied by value, and
// contain *either* a pointer to a garbage-collectable nasl object
// (string, vector, hash) *or* a floating point number.  Keeping the
// number here is an optimization to prevent the generation of
// zillions of tiny "number" object that have to be collected.  Note
// sneaky hack: on little endian systems, placing reftag after ptr and
// putting 1's in the top 13 bits makes the double value a NaN, and
// thus unmistakable (no actual number can appear as a reference).
// Swap the structure order on 32 bit big-endian systems.  On 64 bit
// sytems of either endianness, reftag and the double won't be
// coincident anyway.
#define NASL_REFTAG 0x7ff56789 // == 2,146,789,257 decimal
typedef union {
    double num;
    struct {
        union {
            struct naObj* obj;
            struct naStr* str;
            struct naVec* vec;
            struct naHash* hash;
            struct naCode* code;
            struct naFunc* func;
            struct naClosure* closure;
        } ptr;
        int reftag;
    } ref;
} naRef;

// This is a macro instead of a separate struct to allow compilers to
// avoid padding.  GCC on x86, at least, will always padd the size of
// an embedded struct up to 32 bits.  Doing it this way allows the
// implementing objects to pack in 16 bits worth of data "for free".
#define GC_HEADER \
    unsigned char mark; \
    unsigned char type

// Notes: A CODE object is a compiled set of bytecode instructions.
// What actually gets executed at runtime is a bound FUNC object,
// which combines the raw code with a pointer to a CLOSURE chain of
// namespaces.
enum { T_STR, T_VEC, T_HASH, T_CODE, T_CLOSURE, T_FUNC,
       NUM_NASL_TYPES }; // V. important that this come last!

#define IS_REF(r) ((r).ref.reftag == NASL_REFTAG)
#define IS_NUM(r) ((r).ref.reftag != NASL_REFTAG)
#define IS_NIL(r) (IS_REF((r)) && (r).ref.ptr.obj == 0)
#define IS_STR(r) (IS_REF((r)) && (r).ref.ptr.obj->type == T_STR)
#define IS_VEC(r) (IS_REF((r)) && (r).ref.ptr.obj->type == T_VEC)
#define IS_HASH(r) (IS_REF((r)) && (r).ref.ptr.obj->type == T_HASH)
#define IS_CODE(r) (IS_REF((r)) && (r).ref.ptr.obj->type == T_CODE)
#define IS_FUNC(r) (IS_REF((r)) && (r).ref.ptr.obj->type == T_FUNC)
#define IS_CLOSURE(r) (IS_REF((r)) && (r).ref.ptr.obj->type == T_CLOSURE)
#define IS_CONTAINER(r) (IS_VEC(r)||IS_HASH(r))
#define IS_SCALAR(r) (IS_NUM((r)) || (r).ref.ptr.obj->type == T_STR)

struct naObj {
    GC_HEADER;
};

struct naStr {
    GC_HEADER;
    int len;
    unsigned char* data;
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

// FIXME: multiple code objects can share the same constant pool
struct naCode {
    GC_HEADER;
    unsigned char* byteCode;
    int nBytes;
    naRef* constants;
    int nConstants;
};

struct naFunc {
    GC_HEADER;
    naRef code;
    naRef closure;
};

struct naClosure {
    GC_HEADER;
    naRef namespace;
    naRef next; // parent closure
};

struct naPool {
    int           elemsz;
    int           nblocks;
    struct Block* blocks;
    int           nfree;   // number of entries in the free array
    int           freesz;  // size of the free array
    void**        free;    // pointers to usable elements
};

// FIXME: write these!
void FREE(void* m);
void* ALLOC(int n);
void ERR(char* msg);
void BZERO(void* m, int n);

// Predicates
int naEqual(naRef a, naRef b);
int naTrue(naRef b);

naRef naNil();
int naTypeSize(int type);

// Context-level stuff
struct Context* naNewContext();
void naGarbageCollect();
naRef naParseCode(struct Context* c, char* buf, int len);

// Allocators/generators:
naRef naObj(int type, struct naObj* o);
naRef naNew(struct Context* c, int type);
naRef naNum(double num);
naRef naNewString(struct Context* c);
naRef naNewVector(struct Context* c);
naRef naNewHash(struct Context* c);
naRef naNewCode(struct Context* c);
naRef naNewClosure(struct Context* c, naRef namespace, naRef next);
naRef naNewFunc(struct Context* c, naRef code, naRef closure);

// String utilities:
void naStr_fromdata(naRef dst, unsigned char* data, int len);
int naStr_tonum(naRef str, double* out);
int naStr_numeric(naRef str);
int naStr_parsenum(char* str, int len, double* result);
void naStr_fromnum(naRef dest, double num);
int naStr_equal(naRef s1, naRef s2);
void naStr_substr(naRef dest, naRef str, int start, int len);
void naStr_concat(naRef dest, naRef s1, naRef s2);
unsigned char* naStr_data(naRef s);
int naStr_len(naRef s);
void naStr_gcclean(struct naStr* s);

// Vector utilities:
void naVec_init(naRef vec);
naRef naVec_removelast(naRef vec);
int naVec_append(naRef vec, naRef o);
int naVec_size(naRef v);
void naVec_set(naRef vec, int i, naRef o);
naRef naVec_get(naRef v, int i);
void naVec_gcclean(struct naVec* s);

// Hash utilities:
int naHash_size(naRef h);
void naHash_keys(naRef dst, naRef hash);
void naHash_delete(naRef hash, naRef key);
int naHash_tryset(naRef hash, naRef key, naRef val); // sets if exists
void naHash_set(naRef hash, naRef key, naRef val);
int naHash_get(naRef hash, naRef key, naRef* out);
void naHash_init(naRef hash);
void naHash_gcclean(struct naHash* s);

// GC pool utilities:
void naGC_init(struct naPool* p, int elemsz);
struct naObj* naGC_get(struct naPool* p);
void naGC_mark(naRef r);
void naGC_reap(struct naPool* p);

#endif // _NASL_H
