#include "nasal.h"
#include "data.h"

#define MIN_HASH_SIZE 4

#define EQUAL(a, b) (IDENTICAL(a, b) || naEqual(a, b))

#define HASH_MAGIC 2654435769u

#define INSERT(hh, hkey, hval, hcol) do {                       \
        unsigned int cc = (hcol), iidx=(hh)->size++;            \
        if(iidx < (1<<(hh)->lgalloced)) {                       \
            struct HashNode* hnn = &(hh)->nodes[iidx];          \
            hnn->key = (hkey); hnn->val = (hval);               \
            hnn->next = (hh)->table[cc];                        \
            (hh)->table[cc] = hnn;                              \
        }} while(0)

// Computes a hash code for a given scalar
static unsigned int hashcode(naRef r)
{
    if(IS_NUM(r))
    {
        // Numbers get the number as a hash.  Just use the bits and
        // xor them together.  Note assumption that sizeof(double) >=
        // 2*sizeof(int).
        unsigned int* p = (unsigned int*)&(r.num);
        return p[0] ^ p[1];
    } else if(PTR(r).str->hashcode) {
        return PTR(r).str->hashcode;
    } else {
        // This is Daniel Bernstein's djb2 hash function that I found
        // on the web somewhere.  It appears to work pretty well.
        unsigned int i, hash = 5831;
        unsigned char *data = (void*)naStr_data(r);
        for(i=0; i<naStr_len(r); i++)
            hash = (hash * 33) ^ data[i];
        PTR(r).str->hashcode = hash;
        return hash;
    }
}

// Which column in a given hash does the key correspond to.
static unsigned int hashcolumn(struct HashRec* h, naRef key)
{
    // Multiply by a big number, and take the top N bits.  Note
    // assumption that sizeof(unsigned int) == 4.
    return (HASH_MAGIC * hashcode(key)) >> (32 - h->lgalloced);
}

static struct HashRec* resize(struct naHash* hash)
{
    struct HashRec *h, *h0 = hash->rec;
    int lga, cols, need = h0 ? h0->size - h0->dels : MIN_HASH_SIZE;

    if(need < MIN_HASH_SIZE) need = MIN_HASH_SIZE;
    for(lga=0; 1<<lga <= need; lga++);
    cols = 1<<lga;
    h = naAlloc(sizeof(struct HashRec) +
                cols * (sizeof(struct HashNode*) + sizeof(struct HashNode)));
    naBZero(h, sizeof(struct HashRec) + cols * sizeof(struct HashNode*));

    h->lgalloced = lga;
    h->nodes = (struct HashNode*)(((char*)h)
                                  + sizeof(struct HashRec)
                                  + cols * sizeof(struct HashNode*));
    for(lga=0; h0 != 0 && lga<(1<<h0->lgalloced); lga++) {
        struct HashNode* hn = h0->table[lga];
        while(hn) {
            INSERT(h, hn->key, hn->val, hashcolumn(h, hn->key));
            hn = hn->next;
        }
    }
    naGC_swapfree((void**)&hash->rec, h);
    return h;
}

// Special, optimized version of naHash_get for the express purpose of
// looking up symbols in the local variables hash (OP_LOCAL is by far
// the most common opcode and deserves some special case
// optimization).  Elides all the typing checks that are normally
// required, presumes that the key is a string and has had its
// hashcode precomputed, checks only for object identity, and inlines
// the column computation.
int naHash_sym(struct naHash* hash, struct naStr* sym, naRef* out)
{
    struct HashRec* h = hash->rec;
    if(h) {
        int col = (HASH_MAGIC * sym->hashcode) >> (32 - h->lgalloced);
        struct HashNode* hn = h->table[col];
        while(hn) {
            if(PTR(hn->key).str == sym) {
                *out = hn->val;
                return 1;
            }
            hn = hn->next;
        }
    }
    return 0;
}

static struct HashNode* find(struct naHash* hash, naRef key)
{
    struct HashRec* h = hash->rec;
    struct HashNode* hn;
    if(!h) return 0;
    for(hn = h->table[hashcolumn(h, key)]; hn; hn = hn->next)
        if(EQUAL(key, hn->key))
            return hn;
    return 0;
}

// Make a temporary string on the stack
static void tmpStr(naRef* out, struct naStr* str, const char* key)
{
    str->emblen = 0;
    str->data.ref.len = 0;
    str->type = T_STR;
    str->data.ref.ptr = (unsigned char*)key;
    str->hashcode = 0;
    while(key[str->data.ref.len]) str->data.ref.len++;
    *out = naNil();
    SETPTR(*out, str);
}

int naMember_cget(naRef obj, const char* field, naRef* out)
{
    naRef key;
    struct naStr str;
    tmpStr(&key, &str, field);
    return naMember_get(obj, key, out);
}

naRef naHash_cget(naRef hash, char* key)
{
    struct naStr str;
    naRef result, key2;
    tmpStr(&key2, &str, key);
    if(naHash_get(hash, key2, &result))
        return result;
    return naNil();
}

void naHash_cset(naRef hash, char* key, naRef val)
{
    struct naStr str;
    naRef key2;
    tmpStr(&key2, &str, key);
    naHash_tryset(hash, key2, val);
}

int naHash_get(naRef hash, naRef key, naRef* out)
{
    if(IS_HASH(hash)) {
        struct HashNode* n = find(PTR(hash).hash, key);
        if(n) { *out = n->val; return 1; }
    }
    return 0;
}

// Simpler version.  Don't create a new node if the value isn't there
int naHash_tryset(naRef hash, naRef key, naRef val)
{
    if(IS_HASH(hash)) {
        struct HashNode* n = find(PTR(hash).hash, key);
        if(n) n->val = val;
        return n != 0;
    }
    return 0;
}

// Special purpose optimization for use in function call setups.  Sets
// a value that is known *not* to be present in the hash table.  As
// for naHash_sym, the key must be a string with a precomputed hash
// code.
void naHash_newsym(struct naHash* hash, naRef* sym, naRef* val)
{
    int col;
    struct HashRec* h = hash->rec;
    while(!h || h->size >= 1<<h->lgalloced)
        h = resize(hash);
    col = (HASH_MAGIC * PTR(*sym).str->hashcode) >> (32 - h->lgalloced);
    INSERT(h, *sym, *val, col);
}

// The cycle check is an integrity requirement for multithreading,
// where raced inserts can potentially cause cycles.  This ensures
// that the "last" thread to hold a reference to an inserted node
// breaks any cycles that might have happened (at the expense of
// potentially dropping items out of the hash).  Under normal
// circumstances, chains will be very short and this will be fast.
static void chkcycle(struct HashNode* node, int count)
{
    struct HashNode* hn = node;
    while(hn && (hn = hn->next) != 0)
        if(count-- <= 0) { node->next = 0; return; }
}

void naHash_set(naRef hash, naRef key, naRef val)
{
    int col;
    struct HashRec* h;
    struct HashNode* n;
    if(!IS_HASH(hash)) return;
    if((n = find(PTR(hash).hash, key))) { n->val = val; return; }
    h = PTR(hash).hash->rec;
    while(!h || h->size >= 1<<h->lgalloced)
        h = resize(PTR(hash).hash);
    col = hashcolumn(h, key);
    INSERT(h, key, val, hashcolumn(h, key));
    chkcycle(h->table[col], h->size - h->dels);
}

void naHash_delete(naRef hash, naRef key)
{
    struct HashRec* h = PTR(hash).hash->rec;
    int col;
    struct HashNode *last=0, *hn;
    if(!IS_HASH(hash) || !h) return;
    col = hashcolumn(h, key);
    hn = h->table[col];
    while(hn) {
        if(EQUAL(hn->key, key)) {
            if(last == 0) h->table[col] = hn->next;
            else last->next = hn->next;
            h->dels++;
            return;
        }
        last = hn;
        hn = hn->next;
    }
}

void naHash_keys(naRef dst, naRef hash)
{
    int i;
    struct HashRec* h = PTR(hash).hash->rec;
    if(!IS_HASH(hash) || !h) return;
    for(i=0; i<(1<<h->lgalloced); i++) {
        struct HashNode* hn = h->table[i];
        while(hn) {
            naVec_append(dst, hn->key);
            hn = hn->next;
        }
    }
}

int naHash_size(naRef hash)
{
    struct HashRec* h = PTR(hash).hash->rec;
    if(!IS_HASH(hash) || !h) return 0;
    return h->size - h->dels;
}

void naHash_gcclean(struct naHash* h)
{
    naFree(h->rec);
    h->rec = 0;
}
