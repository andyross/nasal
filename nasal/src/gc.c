#include "nasl.h"

#define MIN_BLOCK_SIZE 256

struct Block {
    int   size;
    char* block;
};

static void appendfree(struct naPool*p, struct naObj* o)
{
    // Need more space?
    if(p->freesz <= p->nfree) {
        int i, n = 1+((3*p->nfree)>>1);
        void** newf = ALLOC(n * sizeof(void*));
        for(i=0; i<p->nfree; i++)
            newf[i] = p->free[i];
        FREE(p->free);
        p->free = newf;
        p->freesz = n;
    }

    p->free[p->nfree++] = o;
}

static void freeelem(struct naPool* p, struct naObj* o)
{
    // Free any intrinsic (i.e. non-garbage collected) storage the
    // object might have
    switch(o->type) {
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
        FREE(((struct naCode*)o)->byteCode);
        FREE(((struct naCode*)o)->constants);
        break;
    case T_CLOSURE:
        break; // Nothing there
    }

    // And add it to the free list
    appendfree(p, o);
}

void naGC_init(struct naPool* p, int elemsz)
{
    p->elemsz = elemsz;
    p->nblocks = 0;
    p->blocks = 0;
    p->nfree = 0;
    p->freesz = 0;
    p->free = 0;
}

// Grabs a free object.  Returns 0 if none is available (i.e., it's
// time to collect)
struct naObj* naGC_get(struct naPool* p)
{
    if(p->nfree == 0) return 0;
    return p->free[--p->nfree];
}

// Sets the reference bit on the object, and recursively on all
// objects reachable from it.  Clumsy: uses C stack recursion, which
// is slower than it need be and may cause problems on some platforms
// due to the very large stack depths that result.
void naGC_mark(naRef r)
{
    int i;

    if(IS_NUM(r) || IS_NIL(r))
        return;

    if(r.ref.ptr.obj->mark == 1)
        return;

    r.ref.ptr.obj->mark = 1;
    switch(r.ref.ptr.obj->type) {
    case T_VEC:
        for(i=0; i<r.ref.ptr.vec->size; i++)
            naGC_mark(r.ref.ptr.vec->array[i]);
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
        for(i=0; i<r.ref.ptr.code->nConstants; i++)
            naGC_mark(r.ref.ptr.code->constants[i]);
        break;
    case T_CLOSURE:
        naGC_mark(r.ref.ptr.closure->code);
        naGC_mark(r.ref.ptr.closure->namespace);
        break;
    }
}

// Collects all the unreachable objects into a free list, and
// allocates more space if needed.
void naGC_reap(struct naPool* p)
{
    int i, elem, total = 0;
    p->nfree = 0;
    for(i=0; i<p->nblocks; i++) {
        struct Block* b = p->blocks + i;
        total += b->size;
        for(elem=0; elem < b->size; elem++) {
            struct naObj* o = (struct naObj*)(b->block + elem * p->elemsz);
            if(o->mark == 0)
                freeelem(p, o);

            // And clear the mark for the next time
            o->mark = 0;
        }
    }

    // Allocate more if necessary (try to keep 33% available)
    if(3*p->nfree <= total) {
        char* buf;
        struct Block* newblocks;

        int used = total - p->nfree;
        int need = 3*used/2 - total;
        if(need < 0)
            return;
        if(need < MIN_BLOCK_SIZE)
            need = MIN_BLOCK_SIZE;

        newblocks = ALLOC((p->nblocks+1) * sizeof(struct Block));
        for(i=0; i<p->nblocks; i++) newblocks[i] = p->blocks[i];
        FREE(p->blocks);
        p->blocks = newblocks;
        buf = ALLOC(need * p->elemsz);
        BZERO(buf, need * p->elemsz);
        p->blocks[p->nblocks].size = need;
        p->blocks[p->nblocks].block = buf;
        p->nblocks++;

        for(i=0; i<need; i++)
            appendfree(p, (struct naObj*)(buf + i*p->elemsz));
    }
}

