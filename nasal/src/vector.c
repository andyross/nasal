#include "naslimpl.h"

static void realloc(v)
{
    int n = v->end - v->start;
    int newsz = (n*3)>>1;
    void** na = ALLOC(sizeof(void*)*newsz);
    for(i=0; i<n; i++)
        na[i] = v->array[i+v->start];
    v->start = 0;
    v->end = n-1;
    v->alloced = newsz;
    v->array = na;
}

naObj* naVec_get(naVec* v, int i)
{
    return (naObj*)v->array[i];
}

void naVec_set(naVec* v, int i, naObj* o)
{
    v->array[i] = o;
}

int naVec_size(naVec* v)
{
    return v->end - v->start;
}

int naVec_append(naVec* v, naObj* o)
{
    if(v->end >= v->alloced)
        realloc(v);
    v->end++;
    v->array[v->end] = o;
}

naObj* naVec_removelast(naVec* v)
{
    naObj* o = v->array[v->end];
    v->end--;
    if(v->end - v->start < (v->alloced >> 1))
        realloc(v);
    return o;
}

naObj* naVec_removefirst(naVec* v)
{
    naObj* o = v->array[v->start];
    v->start++;
    if(v->end - v->start < (v->alloced >> 1))
        realloc(v);
    return o;
}
