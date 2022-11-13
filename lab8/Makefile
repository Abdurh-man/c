#Makefile for threaded jtalk (jtalk_server, lab8)
#Updated 11/27/2005 to work w/ Linux
CC = gcc
CLASSDIR = /home/huangj/cs360
INCLUDES = $(CLASSDIR)/include
CFLAGS = -g -I$(INCLUDES)
LIBDIR = /home/cs360/pub
LIBS = $(LIBDIR)/socketfun.o $(LIBDIR)/libfdr.a -lpthread
EXECUTABLES = jtalk_server

all: $(EXECUTABLES)

clean:
	rm -f core $(EXECUTABLES) *.o

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -c $*.c

jtalk_server: jtalk_server.o
	$(CC) -g -o jtalk_server jtalk_server.o $(LIBS)
