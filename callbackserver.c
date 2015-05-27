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
 * Callback server over SRPC
 *
 * main thread
 *   creates a service named Callback
 *   expects queries of two types
 *     "connect host port service" encoded as an EOS-terminated string
 *     "disconnect host port service" encoded as an EOS-terminated string
 *     connect causes a connection back to host/port/service to be created
 *       and generates a random time interval between event messages for that
 *       connection
 *       if successful, returns "OK id" encoded as an EOS-terminated string
 *       if not, returns "ERR" encoded as an EOS-terminated string
 *     disconnect causes one of these connections to be removed
 *       if host/port/service does not correspond to an active connection,
 *          returns "ERR", otherwise returns "OK"
 */

#include "callback.h"
#include "srpc.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#define UNUSED __attribute__ ((unused))

typedef struct cb {
    struct cb *next;
    unsigned long id;
    RpcConnection rpc;
    char *host;
    char *service;
    unsigned short port;
    unsigned short ticks;
    unsigned short ticksLeft;
} Callback;

/*
 * global data shared by main thread and timer thread
 *
 * all access is via critical sections using mutex
 */
static Callback *head = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned long counter = 1l;
static const struct timespec one_tick = {0, 20000000}; /* one tick is 20 ms */

/*
 * timer thread
 *
 * every tick (20ms), goes through the list of callback connections
 * decrementing ticksLeft; if this reaches 0, resets ticksLeft to the event
 * delay, then invokes rpc_call on that callback connection
 * the query sent is "id event occurred" as an EOS-terminated string
 * prints the received response
 */
static void *timer(UNUSED void *args) {
    Callback *p;
    Q_Decl(event,100);
    char resp[100];
    unsigned qlen, rlen;
    unsigned rsize = 100;

    for(;;) {
        if (nanosleep(&one_tick, NULL) != 0)
            break;
        pthread_mutex_lock(&mutex);
        for (p = head; p != NULL; p = p->next) {
            if (--p->ticksLeft <= 0) {
                p->ticksLeft = p->ticks;
                sprintf(event, "%08lx event occurred", p->id);
                qlen = strlen(event) + 1;
                if (! rpc_call(p->rpc, Q_Arg(event), qlen, resp, rsize,&rlen)) {
                    printf("server: error sending event to %08lx\n", p->id);
                } else {
                    printf("server: response from %08lx/%s/%hu/%s: %s\n",
                           p->id, p->host, p->port, p->service, resp);
                }
            }
        }
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

static unsigned long generate_id(void) {
    unsigned long id;

    pthread_mutex_lock(&mutex);
    id = counter++;
    pthread_mutex_unlock(&mutex);
    return id;
}

/*
 * generates a random interval of ticks (20ms) for the period of a connection
 *
 * value between 15 and 30 seconds
 */
static unsigned short generate_ticks(void) {
    unsigned short ticks;

    ticks = 750 + random() % 750;	/* 750 ticks == 15 seconds */

    return ticks;
}

#define CONNECT 1
#define DISCONNECT 2

int main(UNUSED int argc, UNUSED char *argv[]) {
    RpcService rps;
    RpcEndpoint sender;
    RpcConnection rpc;
    unsigned len;
    char query[100], resp[100], cbuf[100], host[100], service[100];
    char *e;
    Callback *pp, *cp;
    int status;
    unsigned short port;
    unsigned long id;
    pthread_t thr;

    /*
     * initialize data structures shared by our threads
     *
     * nothing to do, all initialization done at compile/link time
     */

    /*
     * initialize the RPC system and offer the Callback service
     */
    assert(rpc_init(CB_PORT));
    rps = rpc_offer(CB_NAME);
    if (rps == NULL) {
        fprintf(stderr, "Failure offering %s service\n", CB_NAME);
        exit(-1);
    }
    /*
     * start timer thread
     */
    if (pthread_create(&thr, NULL, timer, NULL)) {
        fprintf(stderr, "Failure to start timer thread\n");
        exit(-1);
    }
    while ((len = rpc_query(rps, &sender, query, 100)) > 0) {
        sscanf(query, "%s %s %hu %s", cbuf, host, &port, service);
        printf("server: query received \"%s %s %hu %s\"\n",
               cbuf, host, port, service);
        status = 0;			/* assume failure */
        if (strcmp(cbuf, "connect") == 0) {
            if ((cp = (Callback *)malloc(sizeof(Callback))) == NULL) {
                e = "memory allocation failure";
            } else if ((rpc = rpc_connect(host, port, service, 1l)) == NULL) {
                e = "failure to connect";
                free(cp);
            } else {
                status = 1;
                id = generate_id();
                cp->rpc = rpc;
                cp->id = id;
                cp->host = strdup(host);
                cp->service = strdup(service);
                cp->port = port;
                cp->ticks = generate_ticks();
                cp->ticksLeft = cp->ticks;
                pthread_mutex_lock(&mutex);
                cp->next = head;
                head = cp;
                pthread_mutex_unlock(&mutex);
            }
        } else if (strcmp(cbuf, "disconnect") == 0) {
            pthread_mutex_lock(&mutex);
            for (cp = head, pp = NULL; cp != NULL; pp = cp, cp = pp->next) {
                if (strcmp(host, cp->host) != 0)
                    continue;
                if (port != cp->port)
                    continue;
                if (strcmp(service, cp->service) != 0)
                    continue;
                if (pp == NULL)		/* unlink from list */
                    head = cp->next;
                else
                    pp->next = cp->next;
                break;
            }
            if (cp != NULL) {
                rpc_disconnect(cp->rpc);
                free(cp->host);
                free(cp->service);
                id = cp->id;
                free(cp);
                status = 1;
            } else {
                e = "unknown callback identifier";
            }
            pthread_mutex_unlock(&mutex);
        } else {
            e = "illegal command";
        }
        if (status) {
            sprintf(resp, "OK %08lx", id);
        } else {
            sprintf(resp, "ERR %s", e);
        }
        len = strlen(resp) + 1;
        printf("server: response sent: %s\n", resp);
        rpc_response(rps, &sender, resp, len);
    }
    exit(-1);
}
