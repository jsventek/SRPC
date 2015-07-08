##########################################################################################
AUTOMAKE_OPTIONS = foreign subdir-objects
ACLOCAL_FLAGS = -I m4

echoclient_LDFLAGS = -L.libs -lsrpc
echoserver_LDFLAGS = -L.libs -lsrpc

bin_PROGRAMS = echoserver echoclient
lib_LTLIBRARIES = libsrpc.la
srpcincludedir = $(includedir)/srpc
srpcinclude_HEADERS = srpc.h endpoint.h

libsrpc_la_SOURCES = crecord.c ctable.c endpoint.c srpc.c tslist.c stable.c

echoserver_SOURCES = echoserver.c
echoserver_DEPENDENCIES = $(lib_LTLIBRARIES)

echoclient_SOURCES = echoclient.c
echoclient_DEPENDENCIES = $(lib_LTLIBRARIES)
