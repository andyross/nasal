#include "parse.h"

void naParseInit(struct Parser* p)
{
    p->buf = 0;
    p->len = 0;
    p->lines = 0;
    p->nLines = 0;
    p->chunks = 0;
    p->chunkSizes = 0;
    p->nChunks = 0;
    p->leftInChunk = 0;
    p->symbolStart = -1;
    p->tree = 0;
}

void naParseDestroy(struct Parser* p)
{
    int i;
    FREE(p->lines);
    for(i=0; i<p->nChunks; i++) FREE(p->chunks[i]);
    FREE(p->chunks);
    FREE(p->chunkSizes);
}

void* naParseAlloc(struct Parser* p, int bytes)
{
    // Round up to 8 byte chunks for alignment
    if(bytes & 0x7) bytes = ((bytes>>3) + 1) << 3;
    
    // Need a new chunk?
    if(p->leftInChunk < bytes) {
        void* newChunk;
        void** newChunks;
        int* newChunkSizes;
        int sz, i;

        sz = p->len;
        if(sz < bytes) sz = bytes;
        newChunk = ALLOC(sz);

        p->nChunks++;

        newChunks = ALLOC(p->nChunks * sizeof(void*));
        for(i=1; i<p->nChunks; i++) newChunks[i] = p->chunks[i-1];
        newChunks[0] = newChunk;

        newChunkSizes = ALLOC(p->nChunks * sizeof(int));
        for(i=1; i<p->nChunks; i++) newChunkSizes[i] = p->chunkSizes[i-1];
        newChunkSizes[0] = sz;

        FREE(p->chunks);
        FREE(p->chunkSizes);
        p->chunks = newChunks;
        p->chunkSizes = newChunkSizes;
        p->leftInChunk = sz;
    }

    char* result = p->chunks[0] + p->chunkSizes[0] - p->leftInChunk;
    p->leftInChunk -= bytes;
    return (void*)result;
}

