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
 * srpc - a simple UDP-based RPC system
 */

#ifndef _SRPC_H_
#define _SRPC_H_

#include "endpoint.h"

typedef void *RpcConnection;
typedef void *RpcService;

/*
 * query descriptor needed to detect buffer overrun problem
 */
struct qdecl {
    int size;
    char *buf;
};

/*
 * query buffers for rpc_call() must be declared
 * and passed using these macros
 */
#define Q_Decl(QUERY,SIZE) char QUERY[SIZE]; \
                           const struct qdecl QUERY ## _struct = {SIZE,QUERY}
#define Q_Arg(QUERY) (&QUERY ## _struct)

/*
 * initialize RPC system - bind to ‘port’ if non-zero
 * otherwise port number assigned dynamically
 * returns 1 if successful, 0 if failure
 */
int rpc_init(unsigned short port);

/*
 * the following methods are used by RPC clients
 */

/*
 * obtain our ip address (as a string) and port number
 */
void rpc_details(char *ipaddr, unsigned short *port);

/*
 * reverse lookup of ip address (as a string) to fully-qualified hostname
 */
void rpc_reverselu(char *ipaddr, char *hostname);

/*
 * send connect message to host:port with initial sequence number
 * svcName indicates the offered service of interest
 * returns 1 after target accepts connect request
 * else returns 0 (failure)
 */
RpcConnection rpc_connect(char *host, unsigned short port,
                          char *svcName, unsigned long seqno);

/*
 * make the next RPC call, waiting until response received
 * must be invoked as rpc_call(rpc, Q_Arg(query), qlen, resp, rsize, &rlen)
 * upon successful return, ‘resp’ contains ‘rlen’ bytes of data
 * returns 1 if successful, 0 otherwise
 */
int rpc_call(RpcConnection rpc, const struct qdecl *query, unsigned qlen,
             void *resp, unsigned rsize, unsigned *rlen);

/*
 * disconnect from target
 * no return
 */
void rpc_disconnect(RpcConnection rpc);

/*
 * the following methods are used to offer and withdraw a named service
 */

/*
 * offer service named `svcName' in this process
 * returns NULL if error
 */
RpcService *rpc_offer(char *svcName);

/*
 * withdraw service
 */
void rpc_withdraw(RpcService rps);

/*
 * the following methods are used by a worker thread in an RPC server
 */

/*
 * obtain the next query message from `rps' - blocks until message available
 * `len' is the size of `qb' to receive the data
 * upon return, ep has opaque sender information
 *              qb has query data
 *
 * returns actual length as function value
 * returns 0 if there is some massive failure in the system
 */
unsigned rpc_query(RpcService rps, RpcEndpoint *ep, void *qb, unsigned len);

/*
 * send the next response message to the ‘ep’
 * ‘rb’ contains the response to return to the caller
 * returns 1 if successful
 * returns 0 if there is a massive failure in the system
 */
int rpc_response(RpcService rps, RpcEndpoint *ep, void *rb, unsigned len);

/*
 * the following methods are used to prevent parent and child processes from
 * colliding over the same port numbers
 */

/*
 * suspends activities of the RPC state machine by locking the connection
 * table
 */
void rpc_suspend();

/*
 * resumes activities of the RPC state machine by unlocking the connection
 * table
 */
void rpc_resume();

/*
 * reinitializes the RPC state machine: purges the connection table, closes
 * the original socket on the original UDP port, creates a new socket and
 * binds it to the new port, finally resumes the RPC state machine
 */
int rpc_reinit(unsigned short port);

/*
 * shutdown the RPC system
 */
void rpc_shutdown(void);

#endif /* _SRPC_H_ */
