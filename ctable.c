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
 * source for connection record routines and data structures
 */

#include "ctable.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#define CTABLE_SIZE 31

static CRecord *cr_by_ep[CTABLE_SIZE];	/* table by endpoint */
static CRecord *cr_by_id[CTABLE_SIZE];	/* table by identifier */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned short ctr = 0;

void ctable_lock(void) {
    pthread_mutex_lock(&mutex);
}

void ctable_unlock(void) {
    pthread_mutex_unlock(&mutex);
}

pthread_mutex_t *ctable_getMutex(void) {
    return &mutex;
}

void ctable_init(void) {
    int i;

    pthread_mutex_init(&mutex, NULL);
    for (i = 0; i < CTABLE_SIZE; i++) {
        cr_by_ep[i] = NULL;
        cr_by_id[i] = NULL;
    }
}

unsigned long ctable_newSubport(void) {
    unsigned long subport;
    pid_t pid;

    if (++ctr > 0x7fff)
        ctr = 1;
    pid = getpid();
    subport = (pid & 0xffff) << 16 | ctr;
    return subport;
}

void ctable_insert(CRecord *cr) {
    unsigned hash = endpoint_hash(cr->ep, CTABLE_SIZE);
    unsigned indx = cr->cid % CTABLE_SIZE;
#ifdef DEBUG
    crecord_dump(cr, "ctable_insert");
#endif /* DEBUG */
    cr->nxt_ep = cr_by_ep[hash];
    cr_by_ep[hash] = cr;
    cr->nxt_id = cr_by_id[indx];
    cr_by_id[indx] = cr;
#ifdef DEBUG
    ctable_dump("ctable_dump  ");
#endif /* DEBUG */
}

CRecord *ctable_look_ep(RpcEndpoint *ep) {
    unsigned hash = endpoint_hash(ep, CTABLE_SIZE);
    CRecord *r, *ans = NULL;

    for (r = cr_by_ep[hash]; r != NULL; r = r->nxt_ep)
        if (endpoint_equal(ep, r->ep)) {
            ans = r;
            break;
        }
#ifdef DEBUG
    if(ans)
        endpoint_dump(ep, "look_ep- found:");
    else
        endpoint_dump(ep, "look_ep-!found:");
#endif /* DEBUG */
    return ans;
}

CRecord *ctable_look_id(unsigned long id) {
    unsigned indx = id % CTABLE_SIZE;
    CRecord *r, *ans = NULL;

    for (r = cr_by_id[indx]; r != NULL; r = r->nxt_id)
        if (id == r->cid) {
            ans = r;
            break;
        }
#ifdef DEBUG
    if(ans)
        debugf("look_id- found: %ld\n", id);
    else
        debugf("look_id-!found: %ld\n", id);
#endif /* DEBUG */
    return ans;
}

/*
 * remove from table - storage returned in timer thread
 */

void ctable_remove(CRecord *cr) {
    CRecord *pr, *cu;
    unsigned hash = endpoint_hash(cr->ep, CTABLE_SIZE);
    unsigned indx = cr->cid % CTABLE_SIZE;

    for (pr = NULL, cu = cr_by_ep[hash]; cu != NULL; pr = cu, cu = pr->nxt_ep) {
        if (cr == cu) {
            if (pr == NULL)
                cr_by_ep[hash] = cu->nxt_ep;
            else
                pr->nxt_ep = cu->nxt_ep;
            break;
        }
    }
    for (pr = NULL, cu = cr_by_id[indx]; cu != NULL; pr = cu, cu = pr->nxt_id) {
        if (cr == cu) {
            if (pr == NULL)
                cr_by_id[indx] = cu->nxt_id;
            else
                pr->nxt_id = cu->nxt_id;
            break;
        }
    }
#ifdef DEBUG
    crecord_dump(cr, "ctable_remove");
#endif /* DEBUG */
}

void ctable_scan(CRecord **retry, CRecord **timed, CRecord **ping, CRecord **purge) {
    CRecord *p, *rty, *tmo, *png, *prg;
    unsigned long st;
    int i;

    rty = NULL;
    tmo = NULL;
    png = NULL;
    prg = NULL;
    for (i = 0; i < CTABLE_SIZE; i++) {
        for (p = cr_by_ep[i]; p != NULL; p = p->nxt_ep) {
            st = p->state;
            if (st == ST_TIMEDOUT) {
                p->link = prg;
                prg = p;
            } else if (st == ST_CONNECT_SENT || st == ST_QUERY_SENT
                       || st == ST_RESPONSE_SENT || st == ST_DISCONNECT_SENT
                       || st == ST_FRAGMENT_SENT || st == ST_SEQNO_SENT) {
                if (--p->ticksLeft <= 0) {
                    if (--p->nattempts <= 0) {
                        p->link = tmo;
                        tmo = p;
                    } else {
                        p->ticks *= 2;
                        p->ticksLeft = p->ticks;
                        p->link = rty;
                        rty = p;
                    }
                }
            } else {
                if (--p->ticksTilPing <= 0) {
                    if (--p->pingsTilPurge <= 0) {
                        p->link = tmo;
                        tmo = p;
#ifdef LOG
                        crecord_dump(p, "No pings: ");
#endif /* LOG */
                    } else {
                        p->ticksTilPing = TICKS_BETWEEN_PINGS;
                        p->link = png;
                        png = p;
                    }
                }
            }
        }
    }
    *retry = rty;
    *timed = tmo;
    *ping = png;
    *purge = prg;
}

void ctable_purge(void) {
    CRecord *p, *next;
    int i;

    (void)pthread_mutex_trylock(&mutex);	/* lock if not already locked */
    pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);
    for (i = 0; i < CTABLE_SIZE; i++) {
        for (p = cr_by_ep[i]; p != NULL; p = next) {
            next = p->nxt_ep;
            crecord_destroy(p);
        }
    }
    ctable_init();
}

void ctable_dump(char *str) {
    CRecord *p;
    int i;

    for (i = 0; i < CTABLE_SIZE; i++) {
        for (p = cr_by_ep[i]; p != NULL; p = p->nxt_ep) {
            crecord_dump(p, str);
        }
    }
}
