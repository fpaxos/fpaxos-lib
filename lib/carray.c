#include "carray.h"
#include <stdlib.h>
#include <assert.h>

struct carray {
	int head;
	int tail;
	int size;
	int count;
	void** array;
};


static int carray_full(struct carray* a);


struct carray* carray_new(int size) {
	struct carray* a;
	a = malloc(sizeof(struct carray));
	assert(a != NULL);
	a->head = 0;
	a->tail = 0;
	a->size = size;
	a->count = 0;
	a->array = malloc(sizeof(void*)*a->size);
	assert(a->array != NULL);
	return a;
}


static void carray_grow(struct carray* a) {
	int i;
	struct carray* tmp = carray_new(a->size * 2);
	for (i = 0; i < carray_count(a); i++)
		carray_push_back(tmp, carray_at(a, i));
	free(a->array);
	a->head = 0;
	a->tail = tmp->tail;
	a->size = tmp->size;
	a->array = tmp->array;
	free(tmp);
}


void carray_free(struct carray* a) {
	free(a->array);
	free(a);
}


static int carray_full(struct carray* a) {
	return a->count == a->size;
}


int carray_empty(struct carray* a) {
	return a->count == 0;
}


int carray_size(struct carray* a) {
	return a->size;
}


int carray_push_back(struct carray* a, void* p) {
	if (carray_full(a))
		carray_grow(a);
	a->array[a->tail] = p;
	a->tail = (a->tail + 1) % a->size;
	a->count++;
	return 0;
}


void* carray_front(struct carray* a) {
	return a->array[a->head];
}


void* carray_pop_front(struct carray* a) {
	void* p;
	if (carray_empty(a)) return NULL;
	p = a->array[a->head];
	a->head = (a->head + 1) % a->size;
	a->count--;
	return p;
}


int carray_count(struct carray* a) {
	return a->count;
}


void* carray_at(struct carray* a, int i) {
	if (carray_empty(a)) return NULL;
	return a->array[(a->head+i) % a->size];
}


void* carray_first_match(struct carray* a, int(*match_fn)(void*, void*), void* arg) {
	int i;
	void* p;
	for (i = 0; i < carray_count(a); i++) {
		p = carray_at(a, i);
		if (match_fn(arg, p))
			return p;
	}
	return NULL;
}


int carray_count_match(struct carray* a, int(*match_fn)(void*, void*), void* arg) {
	int i, count = 0;
	for (i = 0; i < carray_count(a); i++)
		if (match_fn(arg, carray_at(a, i)))
			count++;
	return count;
}


static struct carray*
_carray_map(struct carray* a, int(*match_fn)(void*, void*), void* arg, int match) {
	int i;
	void* p;
	struct carray* new = carray_new(a->size);
	for (i = 0; i < carray_count(a); i++) {
		p = carray_at(a, i);
		if (match_fn(arg, p) == match)
			carray_push_back(new, p);
	}
	return new;
}


struct carray* carray_collect(struct carray* a, int(*match_fn)(void*, void*), void* arg) {
	return _carray_map(a, match_fn, arg, 1);
}


struct carray* carray_reject(struct carray* a, int(*match_fn)(void*, void*), void* arg) {
	return _carray_map(a, match_fn, arg, 0);
}
