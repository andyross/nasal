#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "nasl.h"

void FREE(void* m) { free(m); }
void* ALLOC(int n) { return malloc(n); }
void ERR(char* msg) { fprintf(stderr, msg); exit(1); }
void BZERO(void* m, int n) { bzero(m, n); }
