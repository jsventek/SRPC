/*
 * Copyright (c) 2013, Court of the University of Glasgow
 * All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:

 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * - Neither the name of the University of Glasgow nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * srpcmalloc - simple malloc and free for use with the SRPC system
 */

#ifndef HEAP_SIZE
#define HEAP_SIZE 10240000
#endif /* HEAP_SIZE */

#ifndef ALIGNMENT
#define ALIGNMENT sizeof(void *)
#endif /* ALIGNMENT */

#include "srpcmalloc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

typedef struct hole {
    size_t size;			/* size of hole NOT including size field */
    struct hole *next;		/* pointer to next hole in the list */
} Hole;

static unsigned char heap[HEAP_SIZE];	/* bytes to use for heap */
static Hole *freeL = (Hole *)heap;
static int init = 1;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *srpc_malloc(size_t size) {
    size_t len = ((size - 1) / ALIGNMENT + 1) * ALIGNMENT + sizeof(size_t);
    unsigned char *p, *result;
    Hole *prev, *cur;

    (void)pthread_mutex_lock(&mutex);
    if (init) {
        init = 0;
        freeL->size = HEAP_SIZE;
        freeL->next = NULL;
    }
    result = NULL;
    for (prev = NULL, cur = freeL; cur != NULL; prev = cur, cur = prev->next) {
        if (cur->size >= len) {		/* FIRST FIT */
            p = (unsigned char *)cur;	/* remember start of hole */
            if ((cur->size - len) < sizeof(Hole)) {	/* remove from list */
                if (prev == NULL)
                    freeL = cur->next;
                else
                    prev->next = cur->next;
            } else {			/* replace cur by remaining fragment */
                Hole *frag = (Hole *)(p + len);
                frag->size = cur->size -= len;
                if (prev != NULL)
                    prev->next = frag;
                else
                    freeL = frag;
                frag->next = cur->next;
                cur->size = len;
            }
            result = p + sizeof(size_t);
            break;
        }
    }
    (void) pthread_mutex_unlock(&mutex);
    return (void *)result;
}

void *srpc_calloc(size_t nmemb, size_t size) {
    void *ptr;
    size_t len;

    len = nmemb * ((size - 1) /ALIGNMENT + 1) * ALIGNMENT;
    if ((ptr = srpc_malloc(len)) != NULL)
        (void) memset(ptr, 0, len);
    return (ptr);
}

void srpc_free(void *ptr) {
    unsigned char *p = (unsigned char *)ptr - sizeof(size_t);
    Hole *h = (Hole *)p;
    Hole *prev, *cur;

    if (ptr == NULL || p < heap || p >= (heap + HEAP_SIZE))
        return;
    /*
     * find spot in list where freed node should go
     */
    (void) pthread_mutex_lock(&mutex);
    for (prev = NULL, cur = freeL; cur < h; prev = cur, cur = prev->next)
        ;
    /*
     * four possibilities at this juncture depending upon values for prev/cur
     * NULL/NULL	free list was empty, simply add this node to list
     * NULL/!NULL	either merge with cur or insert at beginning of list
     * !NULL/NULL	either merge with prev or append to tail of list
     * !NULL/!NULL	either merge with prev, merge with cur, or merge with both
     */
    if (cur == NULL) {		/* first attempt merger with cur */
        h->next = NULL;		/* at tail of free list */
    } else if ((p + h->size) == (unsigned char *)cur) {
        h->size += cur->size;	/* merge with cur */
        h->next = cur->next;
    } else {
        h->next = cur;		/* just link to cur */
    }
    if (prev == NULL) {		/* now attempt merger with prev */
        freeL = h;		/* at head of free list */
    } else if (((unsigned char *)prev + prev->size) == p) {
        prev->size += h->size;	/* merge with prev */
        prev->next = h->next;
    } else {
        prev->next = h;		/* just link to prev */
    }
    (void) pthread_mutex_unlock(&mutex);
}

void srpc_dump(void) {
    Hole *p;
    fprintf(stderr, "Current state of srpc_malloc free list\n");
    (void) pthread_mutex_lock(&mutex);
    for (p = freeL; p != NULL; p = p->next) {
        fprintf(stderr, "chunk @ %p has %zd free bytes\n", p, p->size);
    }
    (void) pthread_mutex_unlock(&mutex);
}
