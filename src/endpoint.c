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
 * source file for endpoint ADT used in SRPC
 */

#include "endpoint.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int memeq(void *ss, void *tt, size_t n) {
    unsigned char *s = (unsigned char *)ss;
    unsigned char *t = (unsigned char *)tt;
    while (n-- > 0)
        if (*s++ != *t++)
            return 0;
    return 1;
}

void endpoint_complete(RpcEndpoint *ep, struct sockaddr_in *addr,
                       unsigned long subport) {
    memcpy(&(ep->addr), addr, sizeof(struct sockaddr_in));
    ep->subport = htonl(subport);
}

RpcEndpoint *endpoint_create(struct sockaddr_in *addr, unsigned long subport) {
    RpcEndpoint *ep = (RpcEndpoint *)malloc(sizeof(RpcEndpoint));
    if (ep != NULL) {
        endpoint_complete(ep, addr, subport);
    }
    return ep;
}

RpcEndpoint *endpoint_duplicate(RpcEndpoint *ep) {
    RpcEndpoint *nep = (RpcEndpoint *)malloc(sizeof(RpcEndpoint));
    if (nep != NULL) {
        memcpy(nep, ep, sizeof(RpcEndpoint));
    }
    return nep;
}

int endpoint_equal(RpcEndpoint *ep1, RpcEndpoint *ep2) {
    int answer = 0;		/* assume not equal */
    if (memeq(&(ep1->addr), &(ep2->addr), sizeof(struct sockaddr_in)))
        if (ep1->subport == ep2->subport)
            answer = 1;
    return answer;
}

#define SHIFT 7		/* should be relatively prime to limit */
unsigned endpoint_hash(RpcEndpoint *ep, unsigned limit) {
    unsigned i;
    unsigned char *p;
    unsigned hash = 0;
    p = (unsigned char *)&(ep->addr);
    for (i = 0; i < sizeof(struct sockaddr_in); i++)
        hash = ((SHIFT * hash) + *p++) % limit;
    hash = ((SHIFT * hash) + ep->subport) % limit;
    return hash;
}

void endpoint_dump(RpcEndpoint *ep, char *leadString) {
    unsigned i;
    unsigned char *p;
    fprintf(stderr, "%s", leadString);
    for (i = 0, p = (unsigned char *)ep; i < sizeof(RpcEndpoint); i++)
        fprintf(stderr, " %02x", *p++);
    fprintf(stderr, "\n");
}
