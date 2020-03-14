/*
 * Copyright (c) 2013-2015, University of Lugano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of it
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "carray.h"
#include "paxos_types.h"
#include <stdlib.h>
#include <assert.h>
#include <paxos.h>
#include <stdio.h>
#include "string.h"
#include "paxos_value.h"

struct carray
{
	int head;
	int tail;
	int size;
	int count;
	void** array;
};

static int carray_full(struct carray* a);
static void carray_grow(struct carray* a);
static void* carray_at(struct carray* a, int i);


static void print_bytes_of_value_to_submit(char * ty, char * val, unsigned char * bytes, size_t num_bytes) {
    char* string; //= sprintf("(%*s) %*s = [ ", 15, ty, 16, val);
    asprintf(&string, "(%*s) %*s = [ ", 15, ty, 16, val);
    for (size_t i = 0; i < num_bytes; i++) {
        char* to_add;
        asprintf(&to_add, "%*u ", 3, bytes[i]);
        strcat(string, to_add);
    }
    strcat(string, "]\n");
    paxos_log_debug("%s", string);
}

#define SHOW(T,V) do { T x = V; print_bytes_of_value_to_submit(#T, #V, (unsigned char*) &x, sizeof(x)); } while(0)



struct carray*
carray_new(int size)
{
	struct carray* a;
	a = malloc(sizeof(struct carray));
	assert(a != NULL);
	a->head = 0;
	a->tail = size - 1;
	a->size = size;
	a->count = 0;
	a->array = malloc(sizeof(void*)*a->size);
	assert(a->array != NULL);
	return a;
}

void
carray_free(struct carray* a)
{
	free(a->array);
	free(a);
}

int
carray_empty(struct carray* a)
{
	return a->count == 0;
}

int
carray_size(struct carray* a)
{
	return a->size;
}

void print(struct carray* a) {
    if (a->count!=0) {
        paxos_log_debug("Printing all queued client values:");
        int i = a->head;
        do {
            SHOW(struct paxos_value*, (struct paxos_value*) a->array[i]);
          //  paxos_log_debug("%s", a->array[i]);
            if(i==a->tail)
                break;
            i = (i + 1) % (a->size);
        } while (i != a->head);
    }
}


int
carray_push_back(struct carray* a, void* p)
{
   // struct paxos_value* test = p;
//    assert(test->paxos_value_len > 0);
  //  assert(test->paxos_value_val != NULL);
  //  assert(memcmp(test->paxos_value_val, "", 1));


	if (carray_full(a))
		carray_grow(a);
    a->tail = (a->tail + 1) % a->size;
	a->array[a->tail] = p;
	a->count++;

	print(a);

	//for (int i =0; i < a->size; i ++) {
	//    paxos_log_debug("%s", a->array[i]);
	//}
//
  /*  if (a->tail >= a->head)
    {
        for (int i = a->head; i <= a->tail - 1; i++)
            assert(a->array[i] != NULL);
//            printf("%d ",arr[i]);
    }
    else
    {
        for (int i = a->head; i < a->size; i++)
            assert(a->array[i] != NULL);
//            printf("%d ", arr[i]);

        for (int i = 0; i <= a->tail - 1; i++)
            assert(a->array[i] != NULL);
    }*/

    return 0;
}




void*
carray_pop_front(struct carray* a)
{
	if (carray_empty(a)) return NULL;

    print(a);

	void *p = a->array[a->head];
	a->head = (a->head + 1) % a->size;
	a->count--;



    return p;
}

void
carray_foreach(struct carray* a, void (*carray_cb)(void*))
{
	int i;
	for (i = 0; i < a->count; ++i)
		carray_cb(carray_at(a, i));
}

static int
carray_full(struct carray* a)
{
	return a->count == a->size;
}

static void
carray_grow(struct carray* a)
{
	int i;
	struct carray* tmp = carray_new(a->size * 2);
	for (i = 0; i < a->count; i++)
		carray_push_back(tmp, carray_at(a, i));
	free(a->array);
	a->head = 0;
	a->tail = tmp->tail;
	a->size = tmp->size;
	a->array = tmp->array;
	free(tmp);
}

static void*
carray_at(struct carray* a, int i)
{
	return a->array[(a->head+i) % a->size];
}


int carray_push_front(struct carray* a, void* p){

    if (carray_full(a))
        carray_grow(a);

    a->head = (a->head - 1) % a->size;
    a->array[a->head] = p;
    a->count++;
    return 0;
}

