#ifndef _NASAL_H
#define _NASAL_H
#ifdef __cplusplus
extern "C" {
#endif

// This is a nasal "reference".  They are always copied by value, and
// contain either a pointer to a garbage-collectable nasal object
// (string, vector, hash) or a floating point number.  Keeping the
// number here is an optimization to prevent the generation of
// zillions of tiny "number" object that have to be collected.  Note
// sneaky hack: on little endian systems, placing reftag after ptr and
// putting 1's in the top 13 (except the sign bit) bits makes the
// double value a NaN, and thus unmistakable (no actual number can
// appear as a reference, and vice versa).  Swap the structure order
// on 32 bit big-endian systems.  On 64 bit sytems of either
// endianness, reftag and the double won't be coincident anyway.
#define NASAL_REFTAG 0x7ff56789 // == 2,146,789,257 decimal
typedef union {
    double num;
    struct {
#ifdef NASAL_BIG_ENDIAN_32_BIT
        int reftag; // Big-endian systems need this here!
#endif
        union {
            struct naObj* obj;
            struct naStr* str;
            struct naVec* vec;
            struct naHash* hash;
            struct naCode* code;
            struct naFunc* func;
            struct naClosure* closure;
            struct naCCode* ccode;
        } ptr;
#ifndef NASAL_BIG_ENDIAN_32_BIT
        int reftag; // Little-endian and 64 bit systems need this here!
#endif
    } ref;
} naRef;

typedef struct Context* naContext;
    
// The function signature for an extension function:
typedef naRef (*naCFunction)(naContext ctx, naRef args);

// All Nasal code runs under the watch of a naContext:
naContext naNewContext();

// Parse a buffer in memory into a code object.
naRef naParseCode(struct Context* c, naRef srcFile, int firstLine,
                  char* buf, int len, int* errLine);

// Call a code or function object with the specifed arguments "on" the
// specified object and using the specified hash for the local
// variables.  Any of args, obj or locals may be nil.
naRef naCall(naContext ctx, naRef func, naRef args, naRef obj, naRef locals);

// Throw an error from the current call stack.  This function makes a
// longjmp call to a handler in naCall() and DOES NOT RETURN.  It is
// intended for use in library code that cannot otherwise report an
// error via the return value, and MUST be used carefully.  If in
// doubt, return naNil() as your error condition.
void naRuntimeError(naContext ctx, char* msg);

// Call a method on an object (NOTE: func is a function binding, *not*
// a code object as returned from naParseCode).
naRef naMethod(naContext ctx, naRef func, naRef object);

// Returns a hash containing functions from the Nasal standard library
// Useful for passing as a namespace to an initial function call
naRef naStdLib(naContext c);

// Ditto, with math functions
naRef naMathLib(naContext c);

// Current line number & error message
int naStackDepth(naContext ctx);
int naGetLine(naContext ctx, int frame);
naRef naGetSourceFile(naContext ctx, int frame);
char* naGetError(naContext ctx);

// Type predicates
int naIsNil(naRef r);
int naIsNum(naRef r);
int naIsString(naRef r);
int naIsScalar(naRef r);
int naIsVector(naRef r);
int naIsHash(naRef r);
int naIsCode(naRef r);
int naIsFunc(naRef r);
int naIsCCode(naRef r);

// Allocators/generators:
naRef naNil();
naRef naNum(double num);
naRef naNewString(naContext c);
naRef naNewVector(naContext c);
naRef naNewHash(naContext c);
naRef naNewFunc(naContext c, naRef code);
naRef naNewCCode(naContext c, naCFunction fptr);

// Some useful conversion/comparison routines
int naEqual(naRef a, naRef b);
int naTrue(naRef b);
naRef naNumValue(naRef n);
naRef naStringValue(naContext c, naRef n);

// String utilities:
int naStr_len(naRef s);
unsigned char* naStr_data(naRef s);
naRef naStr_fromdata(naRef dst, unsigned char* data, int len);
naRef naStr_concat(naRef dest, naRef s1, naRef s2);
naRef naStr_substr(naRef dest, naRef str, int start, int len);

// Vector utilities:
int naVec_size(naRef v);
naRef naVec_get(naRef v, int i);
void naVec_set(naRef vec, int i, naRef o);
int naVec_append(naRef vec, naRef o);
naRef naVec_removelast(naRef vec);

// Hash utilities:
int naHash_size(naRef h);
int naHash_get(naRef hash, naRef key, naRef* out);
naRef naHash_cget(naRef hash, char* key);
void naHash_set(naRef hash, naRef key, naRef val);
void naHash_cset(naRef hash, char* key, naRef val);
void naHash_delete(naRef hash, naRef key);
void naHash_keys(naRef dst, naRef hash);

#ifdef __cplusplus
} // extern "C"
#endif
#endif // _NASAL_H
