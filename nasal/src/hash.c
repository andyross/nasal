#include "nasl.h"

struct HashNode {
    naRef key;
    naRef val;
    struct HashNode* next;
};

static void realloc(naRef hash)
{
    struct naHash* h = hash.ref.ptr.hash;
    int i, sz, oldsz = h->size;

    // Keep a handle to our original objects
    struct HashNode* oldnodes = h->nodes;

    // Figure out how big we need to be (start with a minimum size of
    // 16 entries)
    for(i=3; 1<<i < oldsz; i++);
    h->lgalloced = i+1;
    
    // Allocate new ones (note that all the records are allocated in a
    // single chunk, to avoid zillions of tiny node allocations)
    sz = 1<<h->lgalloced;
    h->nodes = ALLOC(sz * (sizeof(struct HashNode) + sizeof(void*)));
    h->table = ((void*)h->nodes) + sz*sizeof(struct HashNode);
    BZERO(h->table, sz * sizeof(void*));
    h->nextnode = 0;
    h->size = 0;

    // Re-insert everything from scratch
    for(i=0; i<oldsz; i++)
        naHash_set(hash, oldnodes[i].key, oldnodes[i].val);

    // Free the old memory
    FREE(oldnodes);
}

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
    } else {
        // This is Daniel Bernstein's djb2 hash function that I found
        // on the web somewhere.  It appears to work pretty well.
        unsigned int i, hash = 5831;
        for(i=0; i<r.ref.ptr.str->len; i++)
            hash = (hash * 33) ^ r.ref.ptr.str->data[i];
        return hash;
    }
}

// Which column in a given hash does the key correspond to.
static unsigned int hashcolumn(struct naHash* h, naRef key)
{
    // Multiply by a big number, and take the top N bits.  Note
    // assumption that sizeof(unsigned int) == 4.
    return (2654435769u * hashcode(key)) >> (32 - h->lgalloced);
}

// True if the two scalars are equal
static char equals(naRef a, naRef b)
{
    if(IS_NUM(a) && IS_NUM(b) && a.num == b.num) return 1;
    return naStr_equal(a, b);
}

struct HashNode* find(struct naHash* h, naRef key)
{
    struct HashNode* hn = h->table[hashcolumn(h, key)];
    while(hn) {
        if(equals(key, hn->key))
            return hn;
        hn = hn->next;
    }
    return 0;
}

void naHash_init(naRef hash)
{
    struct naHash* h = hash.ref.ptr.hash;
    h->size = 0;
    h->lgalloced = 0;
    h->table = 0;
}

naRef naHash_get(naRef hash, naRef key)
{
    struct naHash* h = hash.ref.ptr.hash;
    struct HashNode* n;
    if(!IS_SCALAR(key)) ERR("Hash lookup by non-scalar illegal");
    n = find(h, key);
    if(n) return n->val;
    return NASL_NIL;
}

void naHash_set(naRef hash, naRef key, naRef val)
{
    struct naHash* h = hash.ref.ptr.hash;
    unsigned int col;
    struct HashNode* n;

    if(!IS_SCALAR(key)) ERR("Hash insert by non-scalar illegal");
    n = find(h, key);
    if(n) {
        n->val = val;
        return;
    }

    h->size++;
    if(h->size > 1<<h->lgalloced)
        realloc(hash);

    col = hashcolumn(h, key);
    n = h->nodes + h->nextnode++;
    n->key = key;
    n->val = val;
    n->next = h->table[col];
    h->table[col] = n;
}

void naHash_delete(naRef hash, naRef key)
{
    struct naHash* h = hash.ref.ptr.hash;
    struct HashNode* hn = h->table[hashcolumn(h, key)];
    while(hn->next) {
        if(equals(hn->next->key, key)) {
            hn->next = hn->next->next;
            h->size--;
            if(h->size < 1<<(h->lgalloced - 1))
                realloc(hash);
            return;
        }
        hn = hn->next;
    }
}

void naHash_keys(naRef dst, naRef hash)
{
    struct naHash* h = hash.ref.ptr.hash;
    int i;
    for(i=0; i<(1<<h->lgalloced); i++) {
        struct HashNode* hn = h->table[i];
        while(hn) {
            naVec_append(dst, hn->key);
            hn = hn->next;
        }
    }
}

int naHash_size(naRef h)
{
    return h.ref.ptr.hash->size;
}

void naHash_destroy(naRef hash)
{
    struct naHash* h = hash.ref.ptr.hash;
    FREE(h->nodes);
    h->nodes = 0;
    h->size = 0;
    h->lgalloced = 0;
}
