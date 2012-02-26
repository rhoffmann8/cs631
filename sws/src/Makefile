CFLAGS=-Wall -Werror -fPIC
LDFLAGS=-Wl,-rpath,.

LIBOBJS=content_type.o files.o log.o list.o parse.o request.o response.o server.o utils.o
SWSOBJS=main.o

LIBRARY=libsws.so
PROGRAM=sws

all: lib ${PROGRAM}

lib: ${LIBRARY}

${LIBRARY}: ${LIBOBJS}
	${CC} ${CFLAGS} -shared ${LIBOBJS} -o $@

${PROGRAM}: ${SWSOBJS}
	${CC} ${CFLAGS} ${SWSOBJS} ${LDFLAGS} -o $@ -L. -lsws

clean:
	rm -f ${LIBOBJS} ${SWSOBJS} ${LIBRARY} ${PROGRAM}
	rm -f *~