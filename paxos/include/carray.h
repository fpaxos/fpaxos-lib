/*
	Copyright (C) 2013 University of Lugano

	This file is part of LibPaxos.

	LibPaxos is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Libpaxos is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with LibPaxos.  If not, see <http://www.gnu.org/licenses/>.
*/


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
int carray_push_front(struct carray* a, void* p);
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
