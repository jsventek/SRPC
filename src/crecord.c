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

#include "crecord.h"
#include "ctable.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char *statenames[] = {
    "", "IDLE", "QACK_SENT", "RESPONSE_SENT", "CONNECT_SENT", "QUERY_SENT",
    "AWAITING_RESPONSE", "TIMEDOUT", "DISCONNECT_SENT", "FRAGMENT_SENT",
    "FACK_RECEIVED", "FRAGMENT_RECEIVED", "FACK_SENT", "SEQNO_SENT"
};

CRecord *crecord_create(RpcEndpoint *ep, unsigned long seqno) {
    CRecord *cr = (CRecord *)malloc(sizeof(CRecord));
    if (cr) {
        cr->nxt_ep = NULL;
        cr->nxt_id = NULL;
        cr->link = NULL;
        cr->mutex = ctable_getMutex();
        pthread_cond_init(&cr->stateChanged, NULL);
        cr->ep = ep;
        cr->cid = 0;
        cr->svc = NULL;
        cr->pl = NULL;
        cr->resp = NULL;
        cr->size = 0;
        cr->nattempts = 0;
        cr->ticks = 0;
        cr->ticksLeft = 0;
        cr->state = 0;
        cr->seqno = seqno;
        cr->lastFrag = 0;
        cr->pingsTilPurge = PINGS_BEFORE_PURGE;
        cr->ticksTilPing = TICKS_BETWEEN_PINGS;
    }
    return (cr);
}

void crecord_dump(CRecord *cr, char *leadString) {
    endpoint_dump(cr->ep, leadString);
    fprintf(stderr, "seqno: %ld, state: %s\n", cr->seqno, statenames[cr->state]);
}

void crecord_setState(CRecord *cr, unsigned long state) {
    cr->state = state;
    cr->ticksTilPing = TICKS_BETWEEN_PINGS;
    cr->pingsTilPurge = PINGS_BEFORE_PURGE;
    pthread_cond_broadcast(&cr->stateChanged);
}

void crecord_setPayload(CRecord *cr, void *pl, unsigned size,
                        unsigned short nattempts, unsigned short ticks) {
    if (cr->pl)
        free(cr->pl);
    cr->pl = pl;
    cr->size = size;
    cr->nattempts = nattempts;
    cr->ticks = ticks;
    cr->ticksLeft = ticks;
}

void crecord_setService(CRecord *cr, SRecord *sr) {
    cr->svc = sr;
}

void crecord_setCID(CRecord *cr, unsigned long id) {
    cr->cid = id;
}

static int matchedState(unsigned long st, unsigned long *states, int n) {
    int i;

    for (i = 0; i < n; i++)
        if (st == states[i])
            return i;
    return n;
}

unsigned long crecord_waitForState(CRecord *cr, unsigned long *states, int n) {
    int i;
    while ((i = matchedState(cr->state, states, n)) == n)
        pthread_cond_wait(&cr->stateChanged, cr->mutex);
    return states[i];
}

void crecord_destroy(CRecord *cr) {
    if (cr) {
        if (cr->ep)
            free(cr->ep);
        if (cr->pl)
            free(cr->pl);
        if (cr->resp)
            free(cr->resp);
        free(cr);
    }
}
