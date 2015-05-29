#!/bin/sh
#
# shell script to generate the SRPC Makefile for the system upon which the
# script is executed
#
# usage: ./genmakefile.sh
#
# if a Makefile exists in the current directory, it is renamed to Makefile.save
# 
# a Makefile customized to the system upon which the script is executed is
# then generated
#
# 

if [ -e ./Makefile ]; then
	echo "Renaming Makefile to Makefile.save"
	mv Makefile Makefile.save
fi
echo "# Makefile for Glasgow SRPC system" >Makefile
echo "# Customized for `uname -n` running `uname -s` on `date`" >>Makefile
echo "# " >>Makefile
echo "OS=`uname -s | sed '/^CYGWIN/s/^.*$/Cygwin/'`" >>Makefile

cat <<!endoftemplate! >>Makefile
# Template makefile for Glasgow SRPC system
#
# Conditionalized upon the value of \$(OS) - if unspecified in the command
# line, OS is assumed to be the value defined above
#
# e.g.
#      make			# makes appropriate binaries for $OS
#      make OS=Cygwin 		# makes appropriate binaries for Cygwin
#      make OS=Darwin 		# makes appropriate binaries for OSX
#      make OS=Linux 		# makes appropriate binaries for Linux

# base definitions
CC = gcc
# comment out the next line if you are using valgrind or gdb
OPT=-O2

CFL_BASE = -W -Wall
LFL_COMMON =
CFL_COMMON = \$(CFL_BASE)
# enable debugging with
# CFL_COMMON = \$(CFL_BASE) -DLOG -DVLOG -DWARNING
# CFL_COMMON = \$(CFL_BASE) -g -DLOG -DVLOG -DWARNING -DDEBUG -DVDEBUG
# compile for valgrind with
# CFL_COMMON = \$(CFL_BASE) -g

ifeq (\$(OS),Cygwin)
    EXT=.exe
else
    EXT=
endif

OBJECTS = crecord.o ctable.o endpoint.o srpc.o stable.o tslist.o
PROGRAMS = mthclient\$(EXT) callbackserver\$(EXT) callbackclient\$(EXT) echoserver\$(EXT) echoclient\$(EXT) sinkclient\$(EXT) sgenclient\$(EXT) sinktest\$(EXT) conntest\$(EXT)

LIBS = -pthread
CFLAGS=\$(CFL_COMMON) \$(OPT)
LDFLAGS = \$(LFL_COMMON)

# OS-specific definitions
ifeq (\$(OS),Cygwin)
    LIBS =
endif
ifeq (\$(OS),Darwin)
    CFLAGS = \$(CFL_COMMON) -DHAVE_SOCKADDR_LEN \$(OPT)
endif

all: \$(PROGRAMS)

clean:
	rm -f *.o *.exe *~ *.stackdump \$(PROGRAMS) libsrpc.a

libsrpc.a: \$(OBJECTS)
	rm -f libsrpc.a
	ar r libsrpc.a \$(OBJECTS)
	ranlib libsrpc.a

install: libsrpc.a
	cp libsrpc.a /usr/local/lib/
	cp srpc.h endpoint.h logdefs.h /usr/local/include/

mthclient.o: mthclient.c srpc.h
callbackserver.o: callbackserver.c callback.h srpc.h
callbackclient.o: callbackclient.c callback.h srpc.h
echoserver.o: echoserver.c srpc.h
echoclient.o: echoclient.c srpc.h
sinkclient.o: sinkclient.c srpc.h
sgenclient.o: sgenclient.c srpc.h
sinktest.o: sinktest.c srpc.h
conntest.o: conntest.c srpc.h
crecord.o: crecord.c crecord.h ctable.h endpoint.h stable.h
ctable.o: ctable.c ctable.h endpoint.h crecord.h
endpoint.o: endpoint.c endpoint.h
srpc.o: srpc.c srpc.h srpcdefs.h tslist.h endpoint.h ctable.h crecord.h stable.h
stable.o: stable.c stable.h tslist.h
tslist.o: tslist.c tslist.h

mthclient\$(EXT): mthclient.o libsrpc.a
	gcc -o mthclient\$(EXT) \$(LIBS) mthclient.o libsrpc.a

callbackserver\$(EXT): callbackserver.o libsrpc.a
	gcc -o callbackserver\$(EXT) \$(LIBS) callbackserver.o libsrpc.a

callbackclient\$(EXT): callbackclient.o libsrpc.a
	gcc -o callbackclient\$(EXT) \$(LIBS) callbackclient.o libsrpc.a

echoserver\$(EXT): echoserver.o libsrpc.a
	gcc -o echoserver\$(EXT) \$(LIBS) echoserver.o libsrpc.a

echoclient\$(EXT): echoclient.o libsrpc.a
	gcc -o echoclient\$(EXT) \$(LIBS) echoclient.o libsrpc.a

sinkclient\$(EXT): sinkclient.o libsrpc.a
	gcc -o sinkclient\$(EXT) \$(LIBS) sinkclient.o libsrpc.a

sgenclient\$(EXT): sgenclient.o libsrpc.a
	gcc -o sgenclient\$(EXT) \$(LIBS) sgenclient.o libsrpc.a

sinktest\$(EXT): sinktest.o libsrpc.a
	gcc -o sinktest\$(EXT) \$(LIBS) sinktest.o libsrpc.a

conntest\$(EXT): conntest.o libsrpc.a
	gcc -o conntest\$(EXT) \$(LIBS) conntest.o libsrpc.a

!endoftemplate!
