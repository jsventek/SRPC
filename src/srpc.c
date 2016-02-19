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
 * originally created to support the Homework event cache
 */

#include "srpc.h"
#include "srpcdefs.h"
#include "tslist.h"
#include "endpoint.h"
#include "ctable.h"
#include "crecord.h"
#include "stable.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define CONNECT 1
#define CACK 2
#define QUERY 3
#define QACK 4
#define RESPONSE 5
#define RACK 6
#define DISCONNECT 7
#define DACK 8
#define FRAGMENT 9
#define FACK 10
#define PING 11
#define PACK 12
#define SEQNO 13
#define SACK 14
#define CMD_LOW CONNECT
#define CMD_HIGH SACK		/* change this if commands added */

#define UNUSED __attribute__ ((unused))

static const char *cmdnames[] = {"", "CONNECT", "CACK", "QUERY", "QACK",
                                 "RESPONSE", "RACK", "DISCONNECT", "DACK",
                                 "FRAGMENT", "FACK", "PING", "PACK", "SEQNO",
                                 "SACK"
                                };

typedef struct ph {
    uint32_t subport;	/* 3rd piece of identifier triple */
    uint32_t seqno;	/* sequence number */
    uint16_t command;	/* message type */
    uint8_t fnum;	/* number of this fragment */
    uint8_t nfrags;	/* number of fragments */
} PayloadHeader;

typedef struct dh {
    uint16_t tlen;	/* total length of the data */
    uint16_t flen;	/* length of this fragment */
} DataHeader;

typedef struct cp {		/* control payload */
    PayloadHeader hdr;
} ControlPayload;

typedef struct conp {
    PayloadHeader hdr;
    char sname[1];		/* EOS-terminated service name */
} ConnectPayload;

typedef struct dp {		/* template for data payload */
    PayloadHeader hdr;
    DataHeader dhdr;
    unsigned char data[1];	/* data is address of `len' bytes */
} DataPayload;

#define CP_SIZE sizeof(ControlPayload)

static struct sockaddr_in my_addr;	/* our address information */
static int my_sock;
static char my_name[16];
static unsigned short my_port;
static const struct timespec one_tick = {0, 20000000}; /* one tick is 20 ms */
static pthread_t readThread = NULL;
static pthread_t timerThread = NULL;

#define MAX_CONN_ID 0x7fffffff
#define MIN_CONN_ID 0x10000000

static unsigned long gen_conn_id(void) {
    static unsigned long id = MIN_CONN_ID;
    unsigned long ans = ++id;
    if (id == MAX_CONN_ID)
        id = MIN_CONN_ID;
    return ans;
}

#ifdef LOG
static void dumpsockNpacket(struct sockaddr_in *s, DataPayload *p, char *lstr) {
    unsigned long subport = ntohl(p->hdr.subport);
    unsigned short command = ntohs(p->hdr.command);
    unsigned long seqno = ntohl(p->hdr.seqno);
    unsigned char fnum = p->hdr.fnum;
    unsigned char nfrags = p->hdr.nfrags;
    char *sp = inet_ntoa(s->sin_addr);
    unsigned short pt = ntohs(s->sin_port);
    logf(
        "%s: host/port/subp/cmd/seqno/fnum/nfrags = %s/%05u/%08lx/%s/%ld/%u/%u",
        lstr, sp, pt, subport, cmdnames[command], seqno, fnum, nfrags);
    if (command == QUERY || command == RESPONSE) {
        unsigned len = ntohs(p->dhdr.tlen);
        printf(" [%u bytes]", len);
    } else if (command == CONNECT) {
        ConnectPayload *q = (ConnectPayload *)p;
        printf(" %s", q->sname);
    }
    printf("\n");
}
#endif /* LOG */

/*
 * complete ControlPayload with data from argument list
 * subport is in network order
 * all others are in host order
 */
#define cp_complete(cp,sp,cmd,sn,fn,nfs) {(cp)->hdr.subport=(sp); \
                                          (cp)->hdr.command=htons(cmd); \
                                          (cp)->hdr.seqno=htonl(sn); \
                                          (cp)->hdr.fnum=(fn); \
                                          (cp)->hdr.nfrags=(nfs); }

/*
 * write message to UDP port
 * returns 1 if successful, or 0 if not
 */
static int send_payload(RpcEndpoint *ep, void *p, int size) {
    struct sockaddr_in d_addr;
    int n, len;
    char *b = (char *)p;

#ifdef DROP_1_IN_20
    if ((random() % 20) == 0)
        return 1;
#endif /* DROP_1_IN_20 */
    len = sizeof(d_addr);
    memcpy(&d_addr, &(ep->addr), len);
#ifdef LOG
    dumpsockNpacket(&d_addr, p, "send");
#endif /* LOG */
    n = sendto(my_sock, b, size, 0, (struct sockaddr *)&d_addr, len);
    if (n == -1)
        return 0;
    else
        return 1;
}

/* continuously reads messages from UDP port */
static void *reader(UNUSED void *args) {
    DataPayload *dp;
    char buf[10240];
    struct sockaddr_in c_addr;
    socklen_t len;
    int n;

    debugf("reader thread started\n");
    for(;;) {
        unsigned short cmd;
        unsigned long sb;
        unsigned long seqno;
        unsigned char fnum;
        unsigned char nfrags;
        char *sp;
        unsigned short pt;
        RpcEndpoint ep;
        CRecord *cr;

        len = sizeof(c_addr);
        memset(&c_addr, 0, len);
        n = recvfrom(my_sock, buf, sizeof(buf), 0,
                     (struct sockaddr *)&c_addr, &len);
        if (n < 0)
            continue;
        buf[n] = '\0';
        dp = (DataPayload *)buf;
        cmd = ntohs(dp->hdr.command);
        sb = ntohl(dp->hdr.subport);
        seqno = ntohl(dp->hdr.seqno);
        fnum = dp->hdr.fnum;
        nfrags = dp->hdr.nfrags;
        sp = inet_ntoa(c_addr.sin_addr);
        pt = ntohs(c_addr.sin_port);
        if (cmd >= CMD_LOW && cmd <= CMD_HIGH) {
            logf("%s from %s:%05u:%08lx; seqno = %ld, frag/nfrag = %u/%u\n",
                 cmdnames[cmd], sp, pt, sb, seqno, fnum, nfrags);
        } else {
            errorf("Illegal command received: %d\n", cmd);
            continue;
        }
        endpoint_complete(&ep, &c_addr, sb);
        ctable_lock();
        cr = ctable_look_ep(&ep);
        switch (cmd) {
        case CONNECT: {
            RpcEndpoint *nep;
            ConnectPayload *conp = (ConnectPayload *)dp;
            ControlPayload *p;
            int newcr = 0;
            SRecord *sr;
            sr = stable_lookup(conp->sname);
            if (sr == NULL)
                break;
            if (cr == NULL) {
                nep = endpoint_duplicate(&ep);
                cr = crecord_create(nep, seqno);
                crecord_setCID(cr, gen_conn_id());
                newcr = 1;
            } else if (cr->state != ST_IDLE) {
                fprintf(stderr,
                        "%s from %s:%05u:%08lx; seqno = %ld, frag/nfrag = %u/%u\n",
                        cmdnames[cmd], sp, pt, sb, seqno, fnum, nfrags);
                crecord_dump(cr, "connectrqst");
            }
            if (newcr || cr->state == ST_IDLE) {
                if (newcr) {
                    p = (ControlPayload *)malloc(CP_SIZE);
                    cp_complete(p, nep->subport, CACK, seqno, 1, 1);
                    crecord_setPayload(cr, p, CP_SIZE, ATTEMPTS, TICKS);
                }
                crecord_setService(cr, sr);
                (void) send_payload(cr->ep, cr->pl, cr->size);
                crecord_setState(cr, ST_IDLE);
            }
            if (newcr)
                ctable_insert(cr);
            break;
        }
        case CACK: {
            if ((cr != NULL)) {
                if (seqno == cr->seqno)
                    crecord_setState(cr, ST_IDLE);
            }
            break;
        }
#define NEW 2
#define OLD 1
#define ILL 0
        case QUERY: {
            DataPayload *p = NULL;
            ControlPayload *cp = NULL;
            int dplen, cplen;
            unsigned long state;
            int accept = ILL;

            if (cr == NULL)
                break;
            state = cr->state;
            if ((seqno - cr->seqno) == 1 &&
                    (state == ST_IDLE || state == ST_RESPONSE_SENT)) {
                accept = NEW;
                cr->seqno = seqno;
                p = (DataPayload *)malloc(n);
                dplen = n;
                memcpy(p, buf, n);
            } else if (seqno == cr->seqno && state == ST_FACK_SENT &&
                       (fnum - cr->lastFrag) == 1 &&
                       fnum == nfrags) {
                void *tp;
                unsigned short flen = ntohs(dp->dhdr.flen);
                accept = NEW;
                p = (DataPayload *)cr->resp;
                dplen = sizeof(PayloadHeader) + sizeof(DataHeader) +
                        ntohs(dp->dhdr.tlen);
                cr->resp = NULL;
                tp = (void *)&(p->data[FR_SIZE * (fnum - 1)]);
                memcpy(tp, dp->data, flen);
            } else if (seqno == cr->seqno &&
                       (state == ST_QACK_SENT || state == ST_RESPONSE_SENT)) {
                accept = OLD;
            }
            switch (accept) {
            case NEW:
                cplen = CP_SIZE;
                cp = (ControlPayload *)malloc(cplen);
                cp_complete(cp, ep.subport, QACK, seqno, fnum, nfrags);
                crecord_setPayload(cr, cp, cplen, ATTEMPTS, TICKS);
                (void)send_payload(cr->ep, cp, cplen);
                tsl_append(cr->svc->s_queue, cr->ep, p, dplen);
                crecord_setState(cr, ST_QACK_SENT);
                break;
            case OLD:
                (void)send_payload(cr->ep, cr->pl, cr->size);
                crecord_setState(cr, state);
                break;
            case ILL:
                break;
            }
            break;
        }
        case QACK: {
            if (cr != NULL) {
                if (seqno == cr->seqno)
                    crecord_setState(cr, ST_AWAITING_RESPONSE);
            }
            break;
        }
        case RESPONSE: {
            DataPayload *p = NULL;
            ControlPayload *cp = NULL;
            int cplen;
            unsigned long st;
            unsigned short flen = ntohs(dp->dhdr.flen);

            if (cr == NULL || seqno != cr->seqno)
                break;
            st = cr->state;
            if (st == ST_QUERY_SENT || st == ST_AWAITING_RESPONSE) {
                p = (DataPayload *)malloc(n);
                memcpy(p, buf, n);
                cr->resp = p;
            } else if (st == ST_FACK_SENT && (fnum - cr->lastFrag) == 1 &&
                       fnum == nfrags) {
                p = (DataPayload *)cr->resp;
                memcpy(&(p->data[FR_SIZE * (fnum -1)]), dp->data, flen);
                cr->lastFrag = fnum;
            } else
                break;
            cplen = CP_SIZE;
            cp = (ControlPayload *)malloc(cplen);
            cp_complete(cp, ep.subport, RACK, seqno, fnum, nfrags);
            crecord_setPayload(cr, cp, cplen, ATTEMPTS, TICKS);
            (void)send_payload(cr->ep, cp, cplen);
            crecord_setState(cr, ST_IDLE);
            break;
        }
        case RACK: {
            if (cr != NULL) {
                if (seqno == cr->seqno)
                    crecord_setState(cr, ST_IDLE);
            }
            break;
        }
        case DISCONNECT: {
            ControlPayload cp;		/* always send a DACK */

            cp_complete(&cp, ep.subport, DACK, seqno, 1, 1);
            (void)send_payload(&ep, &cp, CP_SIZE);
            if (cr != NULL) {
                crecord_setState(cr, ST_TIMEDOUT);
            }
            break;
        }
        case DACK: {
            if (cr != NULL) {
                if (seqno == cr->seqno)
                    crecord_setState(cr, ST_TIMEDOUT);
            }
            break;
        }
        case FRAGMENT: {
            DataPayload *p = NULL;
            ControlPayload *cp = NULL;
            int dplen, cplen;
            unsigned long st;
            int accept = ILL;
            unsigned short tlen = ntohs(dp->dhdr.tlen);
            unsigned short flen = ntohs(dp->dhdr.flen);
            int isQ, isR;

            if (cr == NULL)
                break;
            st = cr->state;
            isQ = (st == ST_IDLE || st == ST_RESPONSE_SENT) &&
                  (seqno - cr->seqno) == 1 && fnum == 1;
            isR = (st == ST_QUERY_SENT || st == ST_AWAITING_RESPONSE) &&
                  seqno == cr->seqno && fnum == 1;
            if (isQ || isR) {
                accept = NEW;
                cr->seqno = seqno;
                dplen = sizeof(PayloadHeader) + sizeof(DataHeader) + tlen;
                cr->resp = malloc(dplen);
                p = (DataPayload *)cr->resp;
                memcpy(p, buf, n);
            } else if (seqno == cr->seqno && st == ST_FACK_SENT &&
                       (fnum - cr->lastFrag) == 1) {
                void *tp;
                accept = NEW;
                p = (DataPayload *)cr->resp;
                tp = (void *)&(p->data[FR_SIZE * (fnum - 1)]);
                memcpy(tp, dp->data, flen);
            } else if (seqno == cr->seqno && st == ST_FACK_SENT &&
                       fnum == cr->lastFrag) {
                accept = OLD;
            }
            switch (accept) {
            case NEW:
                cr->lastFrag = fnum;
                cplen = CP_SIZE;
                cp = (ControlPayload *)malloc(cplen);
                cp_complete(cp, ep.subport, FACK, seqno, fnum, nfrags);
                crecord_setPayload(cr, cp, cplen, ATTEMPTS, TICKS);
                (void)send_payload(cr->ep, cp, cplen);
                crecord_setState(cr, ST_FACK_SENT);
                break;
            case OLD:
                (void)send_payload(cr->ep, cr->pl, cr->size);
                crecord_setState(cr, st);
                break;
            case ILL:
                break;
            }
            break;
        }
        case FACK: {
            if (cr != NULL) {
                if (seqno == cr->seqno && cr->state == ST_FRAGMENT_SENT
                        && fnum == cr->lastFrag) {
                    crecord_setState(cr, ST_FACK_RECEIVED);
                }
            }
            break;
        }
        case PING: {
            ControlPayload cp;

            if (cr != NULL) {
                cp_complete(&cp, ep.subport, PACK, seqno, 1, 1);
                (void)send_payload(&ep, &cp, CP_SIZE);
            }
            break;
        }
        case PACK: {
            if (cr != NULL) {
                crecord_setState(cr, cr->state);	/* resets ping data */
            }
            break;
        }
        case SEQNO: {
            ControlPayload *cp;

            if (cr != NULL) {
                unsigned long st = cr->state;
                if (st == ST_IDLE || st == ST_RESPONSE_SENT) {
                    cp = (ControlPayload *)malloc(CP_SIZE);
                    cp_complete(cp, ep.subport, SACK, seqno, 1, 1);
                    crecord_setPayload(cr, cp, CP_SIZE, ATTEMPTS, TICKS);
                    (void)send_payload(cr->ep, cp, CP_SIZE);
                    cr->seqno = seqno;
                    crecord_setState(cr, ST_IDLE);
                }
            }
            break;
        }
        case SACK: {
            if (cr != NULL && cr->state == ST_SEQNO_SENT) {
                crecord_setState(cr, ST_IDLE);
            }
            break;
        }
        default: {
            break;
        }
        }
        ctable_unlock();
    }
    return NULL;
}

/* scans active connections every 20ms, retrying CONNECT, QUERY and RESPONSE
 * messages when timer has expired; time between retries increases
 * exponentially (actually, doubles)
 *
 * if number of retry attempts has been exhausted, purges those connections
 * from the table
 */
#define TICKS_TIL_PURGE 10
static void *timer(UNUSED void *args) {
    CRecord *retry, *timed, *ping, *purge, *cr;
    int counter = 0;

    debugf("timer thread started\n");
    for (;;) {
        if (nanosleep(&one_tick, NULL) != 0)
            break;
        ctable_lock();
        if ((++counter % 500) == 0) {
            counter = 0;
#ifdef VLOG
            logvf("Dump of connection table\n");
            ctable_dump("LOGV> ");
#endif /* VLOG */
        }
        ctable_scan(&retry, &timed, &ping, &purge);
        while (purge != NULL) {
            cr = purge->link;
            ctable_remove(purge);
            crecord_destroy(purge);
            purge = cr;
        }
        while (timed != NULL) {
            cr = timed->link;
            crecord_setState(timed, ST_TIMEDOUT);
            timed = cr;
        }
        while (ping != NULL) {
            ControlPayload pl;
            cr = ping->link;
            cp_complete(&pl, ping->ep->subport, PING, ping->seqno, 1, 1);
            (void)send_payload(ping->ep, &pl, CP_SIZE);
            ping = cr;
        }
        while (retry != NULL) {
            cr = retry->link;
            switch(retry->state) {
            case ST_CONNECT_SENT:
            case ST_QUERY_SENT:
            case ST_RESPONSE_SENT:
            case ST_DISCONNECT_SENT:
            case ST_FRAGMENT_SENT:
            case ST_SEQNO_SENT:
                (void)send_payload(retry->ep, retry->pl, retry->size);
                break;
            }
            retry = cr;
        }
        ctable_unlock();
    }
    return NULL;
}

static int common_init(unsigned short port) {
    socklen_t len = sizeof(struct sockaddr_in);

    memset(&my_addr, 0, len);
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    if ((my_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ||
            bind(my_sock, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0)
        return 0;
    getsockname(my_sock, (struct sockaddr *)&my_addr, &len);
    my_port = ntohs(my_addr.sin_port);
    if (pthread_create(&readThread, NULL, reader, NULL))
        return 0;
    if (pthread_create(&timerThread, NULL, timer, NULL))
        return 0;
    return 1;
}

int rpc_init(char *hostname, unsigned short port) {
    char host[128];
    struct hostent *hp;

    debugf("rpc_init() entered\n");
    ctable_init();
    stable_init();

    if (hostname != NULL) {
        hp = gethostbyname(hostname);
    } else {
        gethostname(host, 128);
        hp = gethostbyname(host);
    }

    if (hp != NULL) {
        struct in_addr tmp;
        memcpy(&tmp, hp->h_addr_list[0], hp->h_length);
        strcpy(my_name, inet_ntoa(tmp));
    } else
        strcpy(my_name, "127.0.0.1");

    debugf("rpc_init() hostname resolution: %s %s\n",
          (hostname ? hostname : host), my_name);
    return common_init(port);
}

/*
 * important note - this routine should only be called if the RPC system has
 * been suspended via a call to rpc_suspend(); it is here primarily to enable
 * the rapid snapshot capability required by the Homework cache.
 *
 * normal mode of use
 *
 *     rpc_suspend();
 *     pid = fork();
 *     if (pid != 0) {			// parent branch
 *         if (pid == -1) {		// error
 *             rpc_resume();
 *             // note error
 *         } else {			// wait for child to terminate
 *             int status;
 *             (void) wait(&status);
 *             rpc_resume();
 *             // success or error depending upon value of status
 *         }
 *     } else {                         // child branch
 *         pid_t pid = fork();		// zombie-free zone
 *         if (pid == -1)
 *             exit(1);
 *         else if (pid != 0)
 *             exit(0);
 *         rpc_reinit(new port number);
 *     }
 *
 */
int rpc_reinit(unsigned short port) {

    ctable_purge();
    close(my_sock);
    return common_init(port);
}

void rpc_suspend() {
    ctable_lock();
}

void rpc_resume() {
    ctable_unlock();
}

void rpc_details(char *ipaddr, unsigned short *port) {
    strcpy(ipaddr, my_name);
    *port = my_port;
}

void rpc_reverselu(char *ipaddr, char *hostname) {
    struct hostent *hp;
    struct in_addr addr;

    inet_aton(ipaddr, &addr);
    if ((hp = gethostbyaddr((void *)&addr, sizeof(addr), AF_INET)) != NULL)
        strcpy(hostname, hp->h_name);
    else
        strcpy(hostname, ipaddr);
}

static RpcEndpoint *rpc_socket(char *host, unsigned short port,
                               unsigned long subport) {
    RpcEndpoint *s;
    struct hostent *hp = gethostbyname(host);

    if (hp == NULL)
        s = NULL;
    else
        s = (RpcEndpoint *)malloc(sizeof(RpcEndpoint));
    if (s != NULL) {
        memset(&s->addr, 0, sizeof(struct sockaddr_in));
        memcpy(&(s->addr).sin_addr, hp->h_addr_list[0], hp->h_length);
        (s->addr).sin_family = AF_INET;
        (s->addr).sin_port = htons(port);
#ifdef HAVE_SOCKADDR_LEN
        (s->addr).sin_len = sizeof(struct sockaddr_in);
#endif /* HAVE_SOCKADDR_LEN */
        s->subport = subport;
    }
    return s;
}

RpcConnection rpc_connect(char *host, unsigned short port,
                          char *svcName, unsigned long seqno) {
    ConnectPayload *buf;
    RpcEndpoint *nep;
    int len = sizeof(PayloadHeader) + 1;	/* room for '\0' */
    CRecord *cr;
    unsigned long subport;
    unsigned long states[2] = {ST_IDLE, ST_TIMEDOUT};
    unsigned long id = 0;

    ctable_lock();
    subport = ctable_newSubport();
    nep = rpc_socket(host, port, subport);
    if (nep != NULL) {
        id = gen_conn_id();
        len += strlen(svcName);			/* room for svcName */
        buf = (ConnectPayload *)malloc(len);
        cp_complete((ControlPayload *)buf, nep->subport, CONNECT, seqno, 1, 1);
        strcpy(buf->sname, svcName);
        cr = crecord_create(nep, seqno);
        crecord_setCID(cr, id);
        crecord_setPayload(cr, buf, len, ATTEMPTS, TICKS);
#ifdef LOG
        dumpsockNpacket(&(nep->addr), (DataPayload *)buf, "rpc_connect");
#endif /* LOG */
        (void) send_payload(nep, buf, len);
        crecord_setState(cr, ST_CONNECT_SENT);
        ctable_insert(cr);
        if (crecord_waitForState(cr, states, 2) == ST_TIMEDOUT) {
            ctable_remove(cr);
            crecord_destroy(cr);
            id = 0;
        }
        ctable_unlock();
    }
    return (RpcConnection)id;
}

#define SEQNO_LIMIT 1000000000
#define SEQNO_START 0
int rpc_call(RpcConnection rpc, const struct qdecl *q, unsigned qlen,
             void *resp, unsigned rsize, unsigned *rlen) {
    DataPayload *buf;
    RpcEndpoint *ep;
    unsigned short size = sizeof(PayloadHeader) + sizeof(DataHeader) + qlen;
    unsigned long seqno;
    unsigned long qstates[2] = {ST_IDLE, ST_TIMEDOUT};
    unsigned long fstates[2] = {ST_FACK_RECEIVED, ST_TIMEDOUT};
    int result = 0;
    CRecord *cr;
    unsigned char fnum;
    unsigned char nfrags;
    unsigned blen;
    unsigned char *cp = (unsigned char *)q->buf;

    if (q->size < (int)qlen) {
        fprintf(stderr, "rpc_call() - buffer overrun by caller\n");
        return result;
    }
    ctable_lock();
    if ((cr = ctable_look_id((unsigned long)rpc)) == NULL) {
        ctable_unlock();
        return result;
    }
    ep = cr->ep;
    if (cr->state == ST_IDLE) {
        if (cr->seqno >= SEQNO_LIMIT) {
            ControlPayload *cp;
            cr->seqno = SEQNO_START;
            cp = (ControlPayload *)malloc(CP_SIZE);
            cp_complete(cp, ep->subport, SEQNO, SEQNO_START, 1, 1);
            crecord_setPayload(cr, cp, CP_SIZE, ATTEMPTS, TICKS);
            (void)send_payload(ep, cp, CP_SIZE);
            crecord_setState(cr, ST_SEQNO_SENT);
            if (crecord_waitForState(cr, qstates, 2) == ST_TIMEDOUT) {
                ctable_unlock();
                return result;
            }
        }
        cr->seqno++;
        seqno = cr->seqno;
        nfrags = (qlen - 1) / FR_SIZE + 1;
        for (fnum = 1; fnum < nfrags; fnum++) {
            size = sizeof(PayloadHeader) + sizeof(DataHeader) + FR_SIZE;
            buf = (DataPayload *)malloc(size);
            cp_complete((ControlPayload *)buf, ep->subport, FRAGMENT,
                        seqno, fnum, nfrags);
            buf->dhdr.tlen = htons(qlen);
            buf->dhdr.flen = htons(FR_SIZE);
            memcpy(buf->data, &(cp[FR_SIZE*(fnum-1)]), FR_SIZE);
            cr->lastFrag = fnum;
            crecord_setPayload(cr, buf, size, ATTEMPTS, TICKS);
            (void)send_payload(ep, buf, size);
            crecord_setState(cr, ST_FRAGMENT_SENT);
            if (crecord_waitForState(cr, fstates, 2) == ST_TIMEDOUT) {
                ctable_unlock();
                return result;
            }
        }
        blen = qlen - FR_SIZE * (nfrags - 1);
        size = sizeof(PayloadHeader) + sizeof(DataHeader) + blen;
        buf = (DataPayload *)malloc(size);
        cp_complete((ControlPayload *)buf, ep->subport, QUERY,
                    seqno, fnum, nfrags);
        buf->dhdr.tlen = htons(qlen);
        buf->dhdr.flen = htons(blen);
        memcpy(buf->data, &(cp[FR_SIZE*(fnum-1)]), blen);
        crecord_setPayload(cr, buf, size, ATTEMPTS, TICKS);
        (void)send_payload(ep, buf, size);
        crecord_setState(cr, ST_QUERY_SENT);
        if (crecord_waitForState(cr, qstates, 2) == ST_TIMEDOUT) {
            ctable_unlock();
            return result;
        }
        buf = (DataPayload *)cr->resp;
        cr->resp = NULL;
        if (cr->state == ST_IDLE) {
            size = ntohs(buf->dhdr.tlen);
            if (size <= rsize) {
                memcpy(resp, buf->data, size);
                *rlen = size;
                result = 1;
            } else
                result = 0;
        }
        free(buf);
    }
    ctable_unlock();
    return result;
}

/* disconnect from target
 */
void rpc_disconnect(RpcConnection rpc) {
    CRecord *cr;
    ControlPayload *cp;
    RpcEndpoint *ep;
    //unsigned long states[1] = {ST_TIMEDOUT};

    ctable_lock();
    if ((cr = ctable_look_id((unsigned long)rpc)) == NULL) {
        ctable_unlock();
        return;
    }
    ep = cr->ep;
    cp = (ControlPayload *)malloc(CP_SIZE);
    cp_complete(cp, ep->subport, DISCONNECT, cr->seqno, 1, 1);
    crecord_setPayload(cr, cp, CP_SIZE, ATTEMPTS, TICKS);
    (void) send_payload(ep, cp, CP_SIZE);
    crecord_setState(cr, ST_DISCONNECT_SENT);
    //(void) crecord_waitForState(cr, states, 1);
    ctable_unlock();
}

RpcService *rpc_offer(char *svcName) {
    SRecord *s = stable_create(svcName);
    return (RpcService) s;
}

void rpc_withdraw(UNUSED RpcService rps) {
    /* do nothing for now */
}

unsigned rpc_query(RpcService rps, RpcEndpoint *ep, void *qb, unsigned len) {
    DataPayload *dp;
    RpcEndpoint *tep;
    SRecord *sr = (SRecord *)rps;
    int size;
    unsigned n;

    tsl_remove(sr->s_queue, (void **)&tep, (void **)&dp, &size);
    *ep = *tep;
    n = ntohs(dp->dhdr.tlen);
    if (n <= len)
        memcpy(qb, dp->data, n);
    else
        n = 0;
    free(dp);
    return n;
}

int rpc_response(UNUSED RpcService rps, RpcEndpoint *ep, void *rb,
                 unsigned len) {
    DataPayload *dp;

    int ans = 0;
    CRecord *cr;
    unsigned char *cp = (unsigned char *)rb;
    unsigned char fnum, nfrags;
    int size, blen;
    unsigned long fstates[2] = {ST_FACK_RECEIVED, ST_TIMEDOUT};

    ctable_lock();
    cr = ctable_look_ep(ep);
    if (cr != NULL && cr->state == ST_QACK_SENT) {
        nfrags = (len - 1) / FR_SIZE + 1;
        for (fnum = 1; fnum  < nfrags; fnum++) {
            size = sizeof(PayloadHeader) + sizeof(DataHeader) + FR_SIZE;
            dp = (DataPayload *)malloc(size);
            cp_complete((ControlPayload *)dp, ep->subport, FRAGMENT,
                        cr->seqno, fnum, nfrags);
            dp->dhdr.tlen = htons(len);
            dp->dhdr.flen = htons(FR_SIZE);
            memcpy(dp->data, &(cp[FR_SIZE*(fnum-1)]), FR_SIZE);
            cr->lastFrag = fnum;
            crecord_setPayload(cr, dp, size, ATTEMPTS, TICKS);
            (void)send_payload(ep, dp, size);
            crecord_setState(cr, ST_FRAGMENT_SENT);
            if (crecord_waitForState(cr, fstates, 2) == ST_TIMEDOUT) {
                ctable_unlock();
                return 0;
            }
        }
        blen = len - FR_SIZE * (nfrags - 1);
        size = sizeof(PayloadHeader) + sizeof(DataHeader) + blen;
        dp = (DataPayload *)malloc(size);
        cp_complete((ControlPayload *)dp, ep->subport, RESPONSE,
                    cr->seqno, fnum, nfrags);
        dp->dhdr.tlen = htons(len);
        dp->dhdr.flen = htons(blen);
        memcpy(dp->data, &(cp[FR_SIZE*(fnum-1)]), blen);
        crecord_setPayload(cr, dp, size, ATTEMPTS, TICKS);
        (void)send_payload(cr->ep, dp, size);
        crecord_setState(cr, ST_RESPONSE_SENT);
        ans = 1;
    }
    ctable_unlock();
    return ans;
}

void rpc_shutdown(void) {
    void *status;

    pthread_cancel(timerThread);
    pthread_cancel(readThread);
    pthread_join(timerThread, &status);
    pthread_join(readThread, &status);
}
