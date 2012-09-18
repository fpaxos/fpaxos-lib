#include <stdlib.h>
#include <stdio.h>

#include "libpaxos_priv.h"

static FILE* debug_f = NULL;

//Open the file for writing malloc/free traces
void open_debug_file() {
    if (debug_f == NULL) {
        debug_f = fopen(MALLOC_TRACE_FILENAME, "w");
    }
}

//Close the file for writing malloc/free traces
int close_debug_file() {
    if (debug_f != NULL) {
        fclose(debug_f);
    }
    return 0;
}

//Called instead of malloc when debug is active
// traces the allocation
void * paxos_debug_malloc(size_t size, char* file, int line) {
    void * p = malloc(size);
    if (p == NULL) {
        printf("Malloc failed in %s line %d\n", file, line);
        exit(1);
    }
    
    if (debug_f == NULL) {
        open_debug_file();
    }
    fprintf(debug_f, "malloc %p %s %d %d\n",p, file, line, (int)size);
    return p;
}

//Called instead of free when debug is active, 
// traces the release of memory
void paxos_debug_free(void* p, char* file, int line) {
    fprintf(debug_f, "free   %p %s %d %d\n",p, file, line, -1/*(int)malloc_size(p)*/);
    free(p);
}

//Wrapper for malloc when debug is not active
void* paxos_normal_malloc(size_t size) {
    void * p = malloc(size);
    if (p == NULL) {      
        printf("Malloc failed, out of memory!!!\n");
        exit(1);
    }
    return p;
}

