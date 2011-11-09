CFLAGS=-Wall -Werror -fPIC
LDFLAGS=-Wl,-rpath,.

LIBOBJS=swslib.o
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
