#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "parse.h"

int main(int argc, char** argv)
{
    int i;
    FILE* f;
    struct stat fdat;
    char* buf;
    struct Parser p;
    struct Token* t;

    for(i=1; i<argc; i++) {
        stat(argv[i], &fdat);
        buf = malloc(fdat.st_size);
        f = fopen(argv[i], "r");
        fread(buf, 1, fdat.st_size, f);

        naParseInit(&p);

        p.buf = buf;
        p.len = fdat.st_size;

        naLex(&p);

        t = p.tree;
        while(t) {
            
        }
    }
    return 0;
}
