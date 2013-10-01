/*
	Copyright (c) 2013, University of Lugano
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
    	* Redistributions of source code must retain the above copyright
		  notice, this list of conditions and the following disclaimer.
		* Redistributions in binary form must reproduce the above copyright
		  notice, this list of conditions and the following disclaimer in the
		  documentation and/or other materials provided with the distribution.
		* Neither the name of the copyright holders nor the
		  names of its contributors may be used to endorse or promote products
		  derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF 
	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.	
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
