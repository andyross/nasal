#include "naslimpl.h"

struct HashNode {
    unsigned int key;
    void* val;
    struct naHashNode* next;
};

static void realloc(naHash* h)
{
    int i, sz, oldsz;

    // Keep a handle to our original objects
    char* oldmem = (char*)h->table;
    struct HashNode* oldnodes = h->nodes;
    oldsz = h->size;

    // Allocate new ones (note that all the records are allocated in a
    // single chunk, to avoid zillions of tiny node allocations)
    h->lgalloced++;
    sz = 1<<h->lgalloced;
    h->table = ALLOC(sz*(sizeof(struct HashNode) + sizeof(void*)));
    BZERO(h->table, sz*sizeof(void*));
    h->nodes = (struct HashNode*)((char*)h->table + sz*sizeof(void*));
    h->nextnode = 0;
    h->size = 0;

    // Re-insert everything from scratch
    for(i=0; i<oldsize; i++)
        naHash_set(h, oldnodes[i].key, oldnodes[i].val);

    // Free the old memory
    FREE(oldmem);
}

// Computes a hash code for a given scalar
static unsigned int hashcode(naScalar* s)
{
    // Numbers get the number as a hash.  Just use the bits and xor
    // them together.  Note assumption that sizeof(double) >=
    // 2*sizeof(int).
    if(s->type == T_DIR_NUM)
    {
        unsigned int* p = (unsigned int*)&(s->dir.num);
        return p[0] ^ p[1];
    }

    // This is Daniel Bernstein's djb2 hash function that I found on
    // the web somewhere.  It appears to work pretty well.
    else {
        unsigned int hash = 5831;
        int i, sz = naScalar_size(s);
        for(i=0; i<sz; i++)
            hash = (hash * 33) ^ naScalar_charat(s, i);
        return hash;
    }
}

// Which column in a given hash does the key correspond to.
static unsigned int hashcolumn(naHash* h, naScalar* s)
{
    // Multiply by a big number, and take the top N bits.  Note
    // assumption that sizeof(unsigned int) == 4.
    return (2654435769u * hashcode(s)) >> (32 - h->lgalloced);
}

// True if the two scalars are equal
static char equals(naScalar* a, naScalar* b)
{
    // Short cuts for identity and numeric equality, which the cmp()
    // operator doesn't care about.  Are they actually short? ...
    if(a == b
       || (a->type == T_DIR_NUM && a->dir.str == 0 &&
           b->type == T_DIR_NUM && b->dir.str == 0 &&
           a->dir.num == b->dir.num))
        return 1;
    else
        return naScalar_cmp(a, b) == 0;
}

HashNode* find(naHash* h, naScalar* key)
{
    HashNode* hn = h->table[hashcolumn(h, key)];
    while(hn) {
        if(key == hn->key || equals(key, hn->key))
            return hn;
        hn = hn->next;
    }
    return 0;
}

naObj* naHash_get(naHash* h, naScalar* key)
{
    HashNode* n = find(h, key);
    if(n) return n->val;
    return 0;
}

void naHash_set(naHash* h, naScalar* key, naObj* val)
{
    unsigned int col;
    HashNode* n = find(h, key);
    if(n) {
        n->val = val;
        return;
    }

    h->size++;
    if(h->size > 1<<h->lgalloced)
        realloc(h);

    col = hashcolumn(h, key);
    n = h->nodes[h->nextnode++];
    n->key = key;
    n->val = val;
    n->next = h->table[col];
    h->table[col] = hn;
}

void naHash_delete(naHash* h, naObj* key)
{
    HashNode* hn = h->table[hashcolumn(h, key)];
    while(hn->next) {
        if(hn->next->key == key) {
            hn->next = hn->next->next;
            h->size--;
            if(h->size < 1<<(h->lgalloced - 1))
                realloc(h);
            return;
        }
        hn = hn->next;
    }
}

naVec* naHash_keys(naHash* h)
{
    naVec* v = NEW_VECTOR();
    for(i=0; i<(1<<lgalloced); i++) {
        hn = h->table[i];
        while(hn) {
            naVec_append(v, hn->key);
            hn = hn->next;
        }
    }
    return v;
}

int naHash_size(naHash* h)
{
    return h->size;
}
