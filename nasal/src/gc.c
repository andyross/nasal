struct Block {
    int   size;
    char* block;
};

struct naPool {
    int    elemsz;
    int    nblocks;
    Block* blocks;
    int    nfree;
    int    freesz;
    void** free;
};

static void appendfree(naPool*p, naObj* o)
{
    // Need more space?
    if(p->nfree == p->freesz) {
        int i, n = (3*p->nfree)>>1;
        void** newf = ALLOC(n * sizeof(void*));
        for(i=0; i<p->nfree; i++)
            newf[i] = p->free[i];
        p->freesz = n;
    }

    p->free[p->nfree++] = o;
}

static void freeelem(naPool* p, naObj* o)
{
    // Free any intrinsic storage the object might have
    switch(o->type) {
    case T_DIR_NUM:
    case T_DIR_NONUM:
        FREE(((naScalar*)o)->dir.str);
        break;
    case T_VECTOR:
        FREE(((naVec*)o)->array);
        break;
    case T_HASH:
        FREE(((naHash*)o)->table);
        FREE(((naHash*)o)->nodes);
        break;
    }

    // And add it to the free list
    appendfree(p, o);
}

// Grabs a free object.  Returns 0 if none is available (i.e., it's
// time to collect)
naObj* naGC_get(naPool* p)
{
    if(p->nfree == 0) return 0;
    return p->free[p->nfree--];
}

// Marks all pool contents as unreferenced
void naGC_clearpool(naPool* p)
{
    for(i=0; i<nblocks; i++) {
        Block* b = p->blocks + i;
        for(elem=0; elem < b->size; elem++)
            ((naObj*)(b->block + elem * p->elemsz))->mark = 0;
    }
}

// Sets the reference bit on the object, and recursively on all
// objects reachable from it.  Clumsy: uses C stack recursion, which
// is slower than it need be and may cause problems on some platforms
// due to the very large stack depths that result.
void naGC_mark(naObj* o)
{
    int i;

    if(o->mark == 1)
        return;

    o->mark = 1;
    switch(o->type) {
    case T_SUB:
        naGC_mark(ref);
        break;
    case T_CAT:
        naGC_mark(ref0);
        naGC_mark(ref1);
        break;
    case T_VECTOR:
        for(i=v->start; i<=v->end; i++)
            naGC_mark(v->array[i]);
        break;
    case T_HASH:
        for(i=0; i<h->table; i++) {
            HashNode* hn = h->table[i];
            while(hn) {
                naGC_mark(hn->key);
                naGC_mark(hn->val);
                hn = hn->next;
            }
        }
        break;
    }
}

// Collects all the unreachable objects into a free list, and
// allocates more space if needed.
void naGC_reap(naPool* p)
{
    int i, total = 0;
    p->nfree = 0;
    for(i=0; i<nblocks; i++) {
        Block* b = p->blocks + i;
        total += b->size;
        for(elem=0; elem < b->size; elem++) {
            naObj* o = (naObj*)(b->block + elem * p->elemsz);
            if(o->mark == 0)
                freeelem(p, o);
        }
    }

    // Allocate more if necessary
    if(2*total < 3*p->nfree) {
        char* buf;

        int need = ((3*p->nfree)>>1) - total;
        if(need < MIN_BLOCK_SIZE)
            need = MIN_BLOCK_SIZE;

        Block* newblocks = ALLOC((p->nblocks+1) * sizeof(Block));
        for(i=0; i<p->nblocks; i++) newblocks[i] = p->blocks[i];
        buf = ALLOC(need * p->elemsz);
        p->blocks[p->nblocks].size = need;
        p->blocks[p->nblocks].block = buf;
        p->nblocks++;

        for(i=0; i<need; i++)
            appendfree(p, (naObj*)(buf + i*p->elemsz));
    }
}

