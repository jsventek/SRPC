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
 * tslist.h - public data structures and entry points for thread-safe
 *            linked lists used in RPC system
 *
 * each element in the list consists of two parts: a sockaddr_in (either the
 * sender of the data or the recipient of the response) and the data buffer to
 * send
 */

#ifndef _TSLIST_H_
#define _TSLIST_H_

typedef void *TSList;

/*
 * constructor
 * returns NULL if error
 */
TSList tsl_create();

/*
 * append element to the list
 * returns 0 if failure to append (malloc failure), otherwise 1
 */
int tsl_append(TSList tsl, void *a, void *b, int size);

/*
 * prepend element to the list
 * returns 0 if failure to prepend (malloc failure), otherwise 1
 */
int tsl_prepend(TSList tsl, void *a, void *b, int size);

/*
 * remove first element of the list; thread blocks until successful
 */
void tsl_remove(TSList tsl, void **a, void **b, int *size);

/*
 * remove first element of the list if there, do not block, return 1/0
 */
int tsl_remove_nb(TSList tsl, void **a, void **b, int *size);

#endif /* _TSLIST_H_ */
