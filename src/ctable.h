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
 * interface and data structures for table holding connection records
 *
 * ctable_init(), ctable_lock() assume that the table is not locked
 * ctable_getMutex() works independent of lock status
 * all other methods assume that the table has previously been locked via a
 * call to ctable_lock()
 */
#ifndef _CTABLE_H_
#define _CTABLE_H_

#include "endpoint.h"
#include "crecord.h"
#include <pthread.h>

/*
 * lock the connection table
 */
void ctable_lock(void);

/*
 * unlock the connection table
 */
void ctable_unlock(void);

/*
 * return the address of the table mutex
 * needed by crecord_create()
 */
pthread_mutex_t *ctable_getMutex(void);

/*
 * initialize the data structures for holding connection records
 */
void ctable_init(void);

/*
 * issue a new subport for this process
 */
unsigned long ctable_newSubport(void);

/*
 * insert a connection record
 */
void ctable_insert(CRecord *cr);

/*
 * lookup the connection record associated with a particular endpoint
 *
 * if successful, returns the associated connection record
 * if not, returns NULL
 */
CRecord *ctable_look_ep(RpcEndpoint *ep);

/*
 * lookup the connection record associated with a particular identifier
 *
 * if successful, returns the associated connection record
 * if not, returns NULL
 */
CRecord *ctable_look_id(unsigned long id);

/*
 * remove a connection record
 *
 * there is no return value
 */
void ctable_remove(CRecord *cr);

/*
 * scan table for timer-based processing
 *
 * return retry, timed, ping and purge linked lists
 */
void ctable_scan(CRecord **retry, CRecord **timed,
                 CRecord **ping, CRecord **purge);

/*
 * purge all entries from the table
 */
void ctable_purge(void);

/*
 * dump all entries in the table
 */
void ctable_dump(char *str);

#endif /* _CTABLE_H_*/
