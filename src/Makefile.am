##########################################################################################
AUTOMAKE_OPTIONS = foreign subdir-objects
ACLOCAL_FLAGS = -I m4

TESTS = ./tests.sh
echoclient_LDFLAGS = -L.libs -lsrpc
echoserver_LDFLAGS = -L.libs -lsrpc
callbackclient_LDFLAGS = -L.libs -lsrpc
callbackserver_LDFLAGS = -L.libs -lsrpc
mthclient_LDFLAGS = -L.libs -lsrpc
sgenclient_LDFLAGS = -L.libs -lsrpc
sinkclient_LDFLAGS = -L.libs -lsrpc
sinktest_LDFLAGS = -L.libs -lsrpc
conntest_LDFLAGS = -L.libs -lsrpc

bin_PROGRAMS = echoserver echoclient
noinst_PROGRAMS = callbackclient callbackserver mthclient sgenclient sinkclient sinktest conntest
lib_LTLIBRARIES = libsrpc.la
srpcincludedir = $(includedir)/srpc
srpcinclude_HEADERS = srpc.h endpoint.h

libsrpc_la_SOURCES = crecord.c ctable.c endpoint.c srpc.c tslist.c stable.c

echoclient_SOURCES = echoclient.c
echoclient_DEPENDENCIES = $(lib_LTLIBRARIES)

echoserver_SOURCES = echoserver.c
echoserver_DEPENDENCIES = $(lib_LTLIBRARIES)

callbackclient_SOURCES = callbackclient.c
callbackclient_DEPENDENCIES = $(lib_LTLIBRARIES)

callbackserver_SOURCES = callbackserver.c
callbackserver_DEPENDENCIES = $(lib_LTLIBRARIES)

mthclient_SOURCES = mthclient.c
mthclient_DEPENDENCIES = $(lib_LTLIBRARIES)

sgenclient_SOURCES = sgenclient.c
sgenclient_DEPENDENCIES = $(lib_LTLIBRARIES)

sinkclient_SOURCES = sinkclient.c
sinkclient_DEPENDENCIES = $(lib_LTLIBRARIES)

sinktest_SOURCES = sinktest.c
sinktest_DEPENDENCIES = $(lib_LTLIBRARIES)

conntest_SOURCES = conntest.c
conntest_DEPENDENCIES = $(lib_LTLIBRARIES)
