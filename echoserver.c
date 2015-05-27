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
 * Echo server
 *
 * single-threaded provider of the Echo service using SRPC
 *
 * legal queries and corresponding responses (all characters):
 *   ECHO:EOS-terminated-string --> 1/0
 *   SINK:EOS-terminated-string --> 1/0
 *   SGEN: --> 0/1EOS-terminated-string
 */

#include "srpc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define PORT 20000
#define SERVICE "Echo"
#define USAGE "./echoserver [-p port] [-s service]"

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

int main(int argc, char *argv[]) {
    RpcEndpoint sender;
    char *query = (char *)malloc(65536);
    char *resp = (char *)malloc(65536);
    char cmd[64], rest[65536];
    unsigned len;
    RpcService rps;
    char *service;
    unsigned short port;
    int i, j;

    service = SERVICE;
    port = PORT;
    for (i = 1; i < argc; ) {
        if ((j = i + 1) == argc) {
            fprintf(stderr, "usage: %s\n", USAGE);
            exit(1);
        }
        if (strcmp(argv[i], "-p") == 0)
            port = atoi(argv[j]);
        else if (strcmp(argv[i], "-s") == 0)
            service = argv[j];
        else {
            fprintf(stderr, "Unknown flag: %s %s\n", argv[i], argv[j]);
        }
        i = j + 1;
    }

    assert(rpc_init(port));
    rps = rpc_offer(service);
    if (rps == NULL) {
        fprintf(stderr, "Failure offering Echo service\n");
        exit(-1);
    }
    while ((len = rpc_query(rps, &sender, query, 65536)) > 0) {
        int i;
        query[len] = '\0';
        rest[0] = '\0';
        for (i = 0; query[i] != '\0'; i++) {
            if (query[i] == ':') {
                strcpy(rest, &query[i+1]);
                break;
            } else
                cmd[i] = query[i];
        }
        cmd[i] = '\0';
        if (strcmp(cmd, "ECHO") == 0) {
            resp[0] = '1';
            strcpy(&resp[1], rest);
        } else if (strcmp(cmd, "SINK") == 0) {
            sprintf(resp, "1");
        } else if (strcmp(cmd, "SGEN") == 0) {
            resp[0] = '1';
            sgen(&resp[1]);
        } else {
            sprintf(resp, "0Illegal command %s", cmd);
        }
        rpc_response(rps, &sender, resp, strlen(resp) + 1);
    }
    return 0;
}
