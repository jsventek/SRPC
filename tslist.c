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
 * tslist.c - implementation of thread-safe linked lists for simple RPC system
 */

#include "tslist.h"
#include <stdlib.h>
#include <pthread.h>

typedef struct element {
    struct element *next;
    void *addr;
    void *data;
    int size;
} Element;

typedef struct listhead {
    Element *head;
    Element *tail;
    unsigned long count;
    pthread_mutex_t mutex;
    pthread_cond_t nonempty;
} ListHead;

/*
 * constructor
 * returns NULL if error
 */
TSList tsl_create() {
    ListHead *lh = (ListHead *)malloc(sizeof(ListHead));
    if (lh != NULL) {
        lh->head = NULL;
        lh->tail = NULL;
        lh->count = 0l;
        if (pthread_mutex_init(&(lh->mutex), NULL) ||
                pthread_cond_init(&(lh->nonempty), NULL)) {
            free(lh);
            lh = NULL;
        }
    }
    return (TSList)lh;
}

/*
 * append element to the list
 * returns 0 if failure to append (malloc failure), otherwise 1
 */
int tsl_append(TSList tsl, void *a, void *b, int size) {
    ListHead *lh = (ListHead *)tsl;
    Element *e = (Element *)malloc(sizeof(Element));
    if (e == NULL)
        return 0;
    e->addr = a;
    e->data = b;
    e->size = size;
    e->next = NULL;
    pthread_mutex_lock(&(lh->mutex));
    if (! lh->count++)
        lh->head = e;
    else
        lh->tail->next = e;
    lh->tail = e;
    pthread_cond_signal(&(lh->nonempty));
    pthread_mutex_unlock(&(lh->mutex));
    return 1;
}

/* prepend element to the list
 * returns 0 if failure to prepend (malloc failure), otherwise 1 */
int tsl_prepend(TSList tsl, void *a, void *b, int size) {
    ListHead *lh = (ListHead *)tsl;
    Element *e = (Element *)malloc(sizeof(Element));
    if (e == NULL)
        return 0;
    e->addr = a;
    e->data = b;
    e->size = size;
    pthread_mutex_lock(&(lh->mutex));
    e->next = lh->head;
    lh->head = e;
    if (! lh->count++)
        lh->tail = e;
    pthread_cond_signal(&(lh->nonempty));
    pthread_mutex_unlock(&(lh->mutex));
    return 1;
}

/* remove first element of the list; thread blocks until successful */
void tsl_remove(TSList tsl, void **a, void **b, int *size) {
    ListHead *lh = (ListHead *)tsl;
    Element *e;

    pthread_mutex_lock(&(lh->mutex));
    while (!lh->count)
        pthread_cond_wait(&(lh->nonempty), &(lh->mutex));
    /*
     * at this point there is at least one element in the queue
     */
    e = lh->head;
    lh->head = e->next;
    if (! --lh->count)
        lh->tail = NULL;
    pthread_mutex_unlock(&(lh->mutex));
    *a = e->addr;
    *b = e->data;
    *size = e->size;
    free(e);
}

/* remove first element of the list if there, do not block, return 1/0 */
int tsl_remove_nb(TSList tsl, void **a, void **b, int *size) {
    ListHead *lh = (ListHead *)tsl;
    Element *e;

    pthread_mutex_lock(&(lh->mutex));
    if (!lh->count) {
        pthread_mutex_unlock(&(lh->mutex));
        return 0;
    }
    /*
     * at this point there is at least one element in the queue
     */
    e = lh->head;
    lh->head = e->next;
    if (! --lh->count)
        lh->tail = NULL;
    pthread_mutex_unlock(&(lh->mutex));
    *a = e->addr;
    *b = e->data;
    *size = e->size;
    free(e);
    return 1;
}
