#include "nasal.h"
#include "data.h"
#include "code.h"
#include "thread.h"

#define MIN_BLOCK_SIZE 256

// "type" for an object freed by the collector
#define T_GCFREED 123 // DEBUG

struct Block {
    int   size;
    char* block;
};

static void garbageCollect()
{
    int i;
    struct Context* c = globals->allContexts;
    while(c) {
        for(i=0; i<NUM_NASAL_TYPES; i++)
            c->nfree[i] = 0;
        for(i=0; i < c->fTop; i++) {
            naGC_mark(c->fStack[i].func);
            naGC_mark(c->fStack[i].locals);
        }
        for(i=0; i < c->opTop; i++)
            naGC_mark(c->opStack[i]);
        naGC_mark(c->dieArg);
        naGC_mark(c->temps);
        c = c->nextAll;
    }

    naGC_mark(globals->save);
    naGC_mark(globals->symbols);
    naGC_mark(globals->meRef);
    naGC_mark(globals->argRef);
    naGC_mark(globals->parentsRef);

    // Finally collect all the freed objects
    for(i=0; i<NUM_NASAL_TYPES; i++)
        naGC_reap(&(globals->pools[i]));
}

static void naCode_gcclean(struct naCode* o)
{
    naFree(o->byteCode);  o->byteCode = 0;
    naFree(o->constants); o->constants = 0;
    naFree(o->argSyms);   o->argSyms = 0;
    naFree(o->optArgSyms); o->argSyms = 0;
}

static void naGhost_gcclean(struct naGhost* g)
{
    if(g->ptr) g->gtype->destroy(g->ptr);
    g->ptr = 0;
}

static void freeelem(struct naPool* p, struct naObj* o)
{
    // Free its thread lock, if it has one
    if(o->lock) naFreeLock(o->lock);
    o->lock = 0;

    // Free any intrinsic (i.e. non-garbage collected) storage the
    // object might have
    switch(p->type) {
    case T_STR:
        naStr_gcclean((struct naStr*)o);
        break;
    case T_VEC:
        naVec_gcclean((struct naVec*)o);
        break;
    case T_HASH:
        naHash_gcclean((struct naHash*)o);
        break;
    case T_CODE:
        naCode_gcclean((struct naCode*)o);
        break;
    case T_GHOST:
        naGhost_gcclean((struct naGhost*)o);
        break;
    }

    // And add it to the free list
    o->type = T_GCFREED; // DEBUG
    p->free[p->nfree++] = o;
}

static void newBlock(struct naPool* p, int need)
{
    int i;
    char* buf;
    struct Block* newblocks;

    if(need < MIN_BLOCK_SIZE) need = MIN_BLOCK_SIZE;
    
    // FIXME: store blocks in a list. This is just dumb...
    newblocks = naAlloc((p->nblocks+1) * sizeof(struct Block));
    for(i=0; i<p->nblocks; i++) newblocks[i] = p->blocks[i];
    naFree(p->blocks);
    p->blocks = newblocks;
    buf = naAlloc(need * p->elemsz);
    naBZero(buf, need * p->elemsz);
    p->blocks[p->nblocks].size = need;
    p->blocks[p->nblocks].block = buf;
    p->nblocks++;
    
    if(need > p->freesz - p->freetop) need = p->freesz - p->freetop;
    p->nfree = 0;
    p->free = p->free0 + p->freetop;
    for(i=0; i < need; i++) {
        struct naObj* o = (struct naObj*)(buf + i*p->elemsz);
        o->mark = 0;
        o->lock = 0;
        o->type = T_GCFREED; // DEBUG
        p->free[p->nfree++] = o;
    }
    p->freetop += need;
}

void naGC_init(struct naPool* p, int type)
{
    p->type = type;
    p->elemsz = naTypeSize(type);
    p->nblocks = 0;
    p->blocks = 0;

    p->free0 = p->free = 0;
    p->nfree = p->freesz = p->freetop = 0;
    naGC_reap(p);
}

int naGC_size(struct naPool* p)
{
    int i, total=0;
    for(i=0; i<p->nblocks; i++)
        total += ((struct Block*)(p->blocks + i))->size;
    return total;
}

struct naObj** naGC_get(struct naPool* p, int n, int* nout)
{
    struct naObj** result;
    if(globals->allocCount < 0 || (p->nfree == 0 && p->freetop >= p->freesz)) {
        globals->allocCount = 0;
        garbageCollect();
    }
    if(p->nfree == 0)
        newBlock(p, naGC_size(p)/8);
    n = p->nfree < n ? p->nfree : n;
    *nout = n;
    p->nfree -= n;
    globals->allocCount -= n;
    result = (struct naObj**)(p->free + p->nfree);
    return result;
}

// Sets the reference bit on the object, and recursively on all
// objects reachable from it.  Uses the processor stack for recursion...
void naGC_mark(naRef r)
{
    int i;

    if(IS_NUM(r) || IS_NIL(r))
        return;

    if(r.ref.ptr.obj->mark == 1)
        return;

    // Verify that the object hasn't been freed incorrectly:
    if(r.ref.ptr.obj->type == T_GCFREED) *(int*)0=0; // DEBUG

    r.ref.ptr.obj->mark = 1;
    switch(r.ref.ptr.obj->type) {
    case T_VEC:
        if(r.ref.ptr.vec->rec)
            for(i=0; i<r.ref.ptr.vec->rec->size; i++)
                naGC_mark(r.ref.ptr.vec->rec->array[i]);
        break;
    case T_HASH:
        if(r.ref.ptr.hash->table == 0)
            break;
        for(i=0; i < (1<<r.ref.ptr.hash->lgalloced); i++) {
            struct HashNode* hn = r.ref.ptr.hash->table[i];
            while(hn) {
                naGC_mark(hn->key);
                naGC_mark(hn->val);
                hn = hn->next;
            }
        }
        break;
    case T_CODE:
        naGC_mark(r.ref.ptr.code->srcFile);
        for(i=0; i<r.ref.ptr.code->nConstants; i++)
            naGC_mark(r.ref.ptr.code->constants[i]);
        break;
    case T_FUNC:
        naGC_mark(r.ref.ptr.func->code);
        naGC_mark(r.ref.ptr.func->namespace);
        naGC_mark(r.ref.ptr.func->next);
        break;
    }
}

// Collects all the unreachable objects into a free list, and
// allocates more space if needed.
void naGC_reap(struct naPool* p)
{
    int i, elem, freesz, total = 0;
    p->nfree = 0;
    for(i=0; i<p->nblocks; i++)
        total += (p->blocks+i)->size;
    freesz = total < MIN_BLOCK_SIZE ? MIN_BLOCK_SIZE : total;
    freesz = 3 * freesz / 2;
    if(p->freesz < freesz) {
        naFree(p->free0);
        p->freesz = freesz;
        p->free = p->free0 = naAlloc(sizeof(void*) * p->freesz);
    }

    for(i=0; i<p->nblocks; i++) {
        struct Block* b = p->blocks + i;
        for(elem=0; elem < b->size; elem++) {
            struct naObj* o = (struct naObj*)(b->block + elem * p->elemsz);
            if(o->mark == 0)
                freeelem(p, o);
            o->mark = 0;
        }
    }

    // allocs of this type until the next collection
    globals->allocCount += total/2;
    
    // Allocate more if necessary (try to keep 25-50% of the objects
    // available)
    if(p->nfree < total/4) {
        int used = total - p->nfree;
        int avail = total - used;
        int need = used/2 - avail;
        if(need > 0)
            newBlock(p, need);
    }
    p->freetop = p->nfree;
}


