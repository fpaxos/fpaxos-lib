#ifndef _CARRAY_H_
#define _CARRAY_H_

#ifdef __cplusplus
extern "C" {
#endif

struct carray;

struct carray* carray_new(int size);
void carray_free(struct carray* a);
int carray_empty(struct carray* a);
int carray_size(struct carray* a);
int carray_push_back(struct carray* a, void* p);
void* carray_front(struct carray* a);
void* carray_pop_front(struct carray* a);
int carray_count(struct carray* a);
void* carray_at(struct carray* a, int i);
void* carray_first_match(struct carray* a, int(*match_fn)(void*, void*), void* arg);
int carray_count_match(struct carray* a, int(*match_fn)(void*, void*), void* arg);
struct carray* carray_collect(struct carray* a, int(*match_fn)(void*, void*), void* arg);
struct carray* carray_reject(struct carray* a, int(*match_fn)(void*, void*), void* arg);


#ifdef __cplusplus
}
#endif

#endif
