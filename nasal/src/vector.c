#include "nasl.h"
#include "data.h"

static void realloc(struct naVec* v)
{
    int i, newsz = 1 + ((v->size*3)>>1);
    naRef* na = naAlloc(sizeof(naRef) * newsz);
    v->alloced = newsz;
    for(i=0; i<v->size; i++)
        na[i] = v->array[i];
    naFree(v->array);
    v->array = na;
}

void naVec_init(naRef vec)
{
    struct naVec* v = vec.ref.ptr.vec;
    v->size = 0;
    v->alloced = 0;
}

void naVec_gcclean(struct naVec* v)
{
    naFree(v->array);
    v->size = 0;
    v->alloced = 0;
    v->array = 0;
}

naRef naVec_get(naRef v, int i)
{
    return v.ref.ptr.vec->array[i];
}

void naVec_set(naRef vec, int i, naRef o)
{
    struct naVec* v = vec.ref.ptr.vec;
    if(i >= v->size) return;
    v->array[i] = o;
}

int naVec_size(naRef v)
{
    return v.ref.ptr.vec->size;
}

int naVec_append(naRef vec, naRef o)
{
    struct naVec* v = vec.ref.ptr.vec;
    if(v->size >= v->alloced)
        realloc(v);
    v->array[v->size] = o;
    return v->size++;
}

naRef naVec_removelast(naRef vec)
{
    struct naVec* v = vec.ref.ptr.vec;
    if(v->size == 0) return naNil();
    naRef o = v->array[v->size - 1];
    v->size--;
    if(v->size < (v->alloced >> 1))
        realloc(v);
    return o;
}
