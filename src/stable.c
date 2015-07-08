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
 * source for service table and data structures
 */

#include "stable.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define STABLE_SIZE 13

static SRecord *stable[STABLE_SIZE];	/* bucket listheads */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#define SHIFT 7
static unsigned int hash(char *key) {
    unsigned int h = 0;
    for (; *key; key++)
        h = (SHIFT * h + *key) % STABLE_SIZE;
    return h;
}

void stable_init() {
    int i;

    for (i = 0; i < STABLE_SIZE; i++)
        stable[i] = NULL;
}

SRecord *stable_create(char *serviceName) {
    SRecord *r;
    unsigned int hv;

    if (stable_lookup(serviceName))
        r = NULL;
    else {
        r = (SRecord *)malloc(sizeof(SRecord));
        if (r) {
            r->s_name = strdup(serviceName);
            r->s_next = NULL;
            r->s_queue = tsl_create();
            if (! r->s_queue) {
                free(r->s_name);
                free(r);
                r = NULL;
            } else {
                hv = hash(r->s_name);
                pthread_mutex_lock(&mutex);
                r->s_next = stable[hv];
                stable[hv] = r;
                pthread_mutex_unlock(&mutex);
            }
        }
    }
    return r;
}

SRecord *stable_lookup(char *name) {
    unsigned int hv = hash(name);
    SRecord *r, *ans = NULL;

    pthread_mutex_lock(&mutex);
    for (r = stable[hv]; r != NULL; r = r->s_next)
        if (strcmp(r->s_name, name) == 0) {
            ans = r;
            break;
        }
    pthread_mutex_unlock(&mutex);
    return ans;
}

void stable_remove(SRecord *sr) {
    SRecord *pr, *cu;
    unsigned int hv = hash(sr->s_name);

    pthread_mutex_lock(&mutex);
    for (pr = NULL, cu = stable[hv]; cu != NULL; pr = cu, cu = pr->s_next) {
        if (strcmp(sr->s_name, cu->s_name) == 0) {
            if (pr == NULL)
                stable[hv] = cu->s_next;
            else
                pr->s_next = cu->s_next;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
    /* should destroy the TSList before deallocating structure */
    free(sr);
}

void stable_dump(void) {
    SRecord *p;
    int i;

    pthread_mutex_lock(&mutex);
    printf("Service table:");
    for (i = 0; i < STABLE_SIZE; i++) {
        for (p = stable[i]; p != NULL; p = p->s_next) {
            printf(" %s", p->s_name);
        }
    }
    printf("\n");
    pthread_mutex_unlock(&mutex);
}
