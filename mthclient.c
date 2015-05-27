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
 * multi-threaded client of the Echo service
 */
#include "srpc.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>

#define HOST "localhost"
#define PORT 20000
#define SERVICE "Echo"
#define USAGE "./mthclient [-t nthreads] [-l nlines] [-h host] [-p port] [-s service]"
#define UNUSED __attribute__ ((unused))

char *host = HOST;
char *service = SERVICE;
unsigned short port = PORT;
int nlines = 1000;
int nthreads = 2;

static const char letters[] = "abcdefghijklmnopqrstuvwxyz0123456789";

static void sgen(char *s) {
    int i, j, N;

    N = random() % 75 + 1;
    for (i = 0; i < N; i++) {
        j = random() % 36;
        *s++ = letters[j];
    }
    *s = '\0';
}

static void *client(UNUSED void *args) {
    RpcConnection rpc;
    char buf[128];
    Q_Decl(query,128);
    char resp[128];
    int i;
    unsigned int len;
    struct timeval start, stop;
    unsigned long count = 0;
    unsigned long msec;
    double mspercall;
    pthread_t my_id = pthread_self();

    if (!(rpc = rpc_connect(host, port, service, 1234l))) {
        fprintf(stderr, "Failure to connect to %s at %s:%05u\n",
                service, host, port);
        pthread_exit(NULL);
    }
    gettimeofday(&start, NULL);
    for (i = 0; i < nlines; i++) {
        count++;
        sgen(buf);
        sprintf(query, "ECHO:%s", buf);
        if(!rpc_call(rpc, Q_Arg(query), strlen(query)+1, resp, 128, &len)) {
            fprintf(stderr, "%d'th rpc_call() failed\n", i+1);
            break;
        }
    }
    gettimeofday(&stop, NULL);
    if (stop.tv_usec < start.tv_usec) {
        stop.tv_usec += 1000000;
        stop.tv_sec--;
    }
    msec = 1000 * (stop.tv_sec - start.tv_sec) +
           (stop.tv_usec - start.tv_usec) / 1000;
    mspercall = (double)msec / (double)count;
    fprintf(stderr, "%p: %ld lines Echo'd in %ld.%03ld seconds, %.3fms/call\n",
            (void *)my_id, count, msec/1000, msec % 1000, mspercall);
    rpc_disconnect(rpc);
    return NULL;
}

#define MAX_THREADS 100

int main(int argc, char *argv[]) {
    int i, j;
    pthread_t th[MAX_THREADS];
    void *status;

    for (i = 1; i < argc; ) {
        if ((j = i + 1) == argc) {
            fprintf(stderr, "usage: %s\n", USAGE);
            exit(1);
        }
        if (strcmp(argv[i], "-h") == 0)
            host = argv[j];
        else if (strcmp(argv[i], "-p") == 0)
            port = atoi(argv[j]);
        else if (strcmp(argv[i], "-s") == 0)
            service = argv[j];
        else if (strcmp(argv[i], "-l") == 0)
            nlines = atoi(argv[j]);
        else if (strcmp(argv[i], "-t") == 0) {
            nthreads = atoi(argv[j]);
            if (nthreads > MAX_THREADS)
                nthreads = MAX_THREADS;
        } else {
            fprintf(stderr, "Unknown flag: %s %s\n", argv[i], argv[j]);
        }
        i = j + 1;
    }
    assert(rpc_init(0));
    for (i = 0; i < nthreads; i++)
        if (pthread_create(&th[i], NULL, client, NULL)) {
            fprintf(stderr, "Failure to start client thread\n");
            exit(-1);
        }
    for (i = 0; i < nthreads; i++)
        pthread_join(th[i], &status);
    exit(0);
}
