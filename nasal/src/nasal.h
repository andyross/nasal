#ifndef _NASL_H
#define _NASL_H

// This is a nasl "reference".  They are always copied by value, and
// contain *either* a pointer to a garbage-collectable nasl object
// (string, vector, hash) *or* a floating point number.  Keeping the
// number here is an optimization to prevent the generation of
// zillions of tiny "number" object that have to be collected.
// Note sneaky hack: on little endian systems, placing reftag after
// ptr and putting 1's in the top 13 bits makes the double value a
// NaN, and thus unmistakable.  Swap the byte order *and* the
// structure order on other systems.
#define NASL_REFTAG 0x7ff56789
typedef union {
    double num;
    struct {
        union {
            struct naObj* obj;
            struct naStr* str;
            struct naVec* vec;
            struct naHash* hash;
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

#define TYPE_NASTR  0
#define TYPE_NAVEC  1
#define TYPE_NAHASH 2

#define IS_REF(r) ((r).ref.reftag == NASL_REFTAG)
#define IS_NUM(r) ((r).ref.reftag != NASL_REFTAG)
#define IS_NIL(r) (IS_REF((r)) && (r).ref.ptr.obj == 0)
#define IS_SCALAR(r) IS_NUM((r)) || ((r).ref.ptr.obj->type == TYPE_NASTR)

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

naRef naNum(double num);
naRef naObj(struct naObj* o);

void naStr_fromdata(naRef dst, unsigned char* data, int len);
double naStr_tonum(naRef str);
void naStr_fromnum(naRef dest, double num);
int naStr_equal(naRef s1, naRef s2);
void naStr_substr(naRef dest, naRef str, int start, int len);
void naStr_concat(naRef dest, naRef s1, naRef s2);
unsigned char* naStr_data(naRef s);
int naStr_len(naRef s);

naRef naVec_removelast(naRef vec);
int naVec_append(naRef vec, naRef o);
int naVec_size(naRef v);
void naVec_set(naRef vec, int i, naRef o);
naRef naVec_get(naRef v, int i);

void naHash_destroy(naRef hash);
int naHash_size(naRef h);
void naHash_keys(naRef dst, naRef hash);
void naHash_delete(naRef hash, naRef key);
void naHash_set(naRef hash, naRef key, naRef val);
naRef naHash_get(naRef hash, naRef key);
void naHash_init(naRef hash);

void naGC_init(struct naPool* p, int elemsz);
struct naObj* naGC_get(struct naPool* p);
void naGC_mark(naRef r);
void naGC_reap(struct naPool* p);

#endif // _NASL_H
