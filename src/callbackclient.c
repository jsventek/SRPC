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
 * Callback client over SRPC
 *
 * main thread
 *   creates a service named Handler (or name provided in command line)
 *   spins off handler thread
 *   connects to Callback
 *   sends connect request to Callback server
 *   sleeps for 5 minutes
 *   sends disconnect request to Callback server
 *   exits
 *
 * handler thread
 *   for each received event message
 *     prints the received event message
 *     sends back a response of "OK"
 */

#include "callback.h"
#include "srpc.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#define USAGE "./callbackclient [-h host] [-p port] [-s service] [-t minutes]"
#define UNUSED __attribute__ ((unused))

/*
 * global data shared by main thread and handler thread
 */
static RpcService rps;

/*
 * handler thread
 */
static void *handler(UNUSED void *args) {
    char event[100], resp[100];
    RpcEndpoint sender;
    unsigned len;

    while ((len = rpc_query(rps, &sender, event, 100)) > 0) {
        printf("client: %s\n", event);
        sprintf(resp, "OK");
        len = strlen(resp) + 1;
        rpc_response(rps, &sender, resp, len);
    }
    return (NULL);
}

static struct timespec time_delay = {300, 0};

/*
 *   creates a service named Handler
 *   spins off handler thread
 *   connects to Callback
 *   sends connect request to Callback server
 *   sleeps for the specified time (default 5 minutes)
 *   sends disconnect request to Callback server
 *   exits
 */
int main(int argc, char *argv[]) {
    RpcConnection rpc;
    unsigned rlen;
    Q_Decl(query,100);
    char resp[100], status[100], myhost[100];
    unsigned short myport;
    unsigned long id;
    pthread_t thr;
    int i, j;
    unsigned short port;
    char *target;
    char *service;
    int minutes;

    target = "localhost";
    port = CB_PORT;
    service = "Handler";
    minutes = 5;
    for (i = 1; i < argc; ) {
        if ((j = i + 1) == argc) {
            fprintf(stderr, "usage: %s\n", USAGE);
            exit(1);
        }
        if (strcmp(argv[i], "-h") == 0)
            target = argv[j];
        else if (strcmp(argv[i], "-p") == 0)
            port = atoi(argv[j]);
        else if (strcmp(argv[i], "-s") == 0)
            service = argv[j];
        else if (strcmp(argv[i], "-t") == 0)
            minutes = atoi(argv[j]);
        else {
            fprintf(stderr, "Unknown flag: %s %s\n", argv[i], argv[j]);
        }
        i = j + 1;
    }
    time_delay.tv_sec = 60 * minutes;
    /*
     * initialize the RPC system and offer the Callback service
     */
    assert(rpc_init(0));
    rps = rpc_offer(service);
    if (! rps) {
        fprintf(stderr, "Failure offering %s service\n", service);
        exit(-1);
    }
    rpc_details(myhost, &myport);
    /*
     * start handler thread
     */
    if (pthread_create(&thr, NULL, handler, NULL)) {
        fprintf(stderr, "Failure to start timer thread\n");
        exit(-1);
    }
    /*
     * connect to Callback service
     */
    rpc = rpc_connect(target, port, CB_NAME, 1l);
    if (rpc == NULL) {
        fprintf(stderr, "Error connecting to %s service\n", CB_NAME);
        exit(1);
    }
    /*
     * send connect message to service
     */
    sprintf(query, "connect %s %u %s", myhost, myport, service);
    if (!rpc_call(rpc, Q_Arg(query), strlen(query)+1, resp, 100, &rlen)) {
        fprintf(stderr, "Error issuing connect command\n");
        exit(1);
    }
    printf("client: response to connect command: '%s'\n", resp);
    sscanf(resp, "%s %08lx", status, &id);
    /*
     * sleep for specified number of minutes (default of 5)
     */
    nanosleep(&time_delay, NULL);
    /*
     * issue disconnect command
     */
    sprintf(query, "disconnect %s %u %s", myhost, myport, service);
    if (!rpc_call(rpc, Q_Arg(query), strlen(query)+1, resp, 100, &rlen)) {
        fprintf(stderr, "Error issuing disconnect command\n");
        exit(1);
    }
    printf("client: response to disconnect command: '%s'\n", resp);
    rpc_disconnect(rpc);
    exit(0);
}
