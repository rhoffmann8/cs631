RM=rm -f
CFLAGS=-c -Wall
LIBS=-lls
LIBPATH=.
NAME=libls.a
SRC=cmp.c ls.c print.c util.c
OBJ=$(SRC:.c=.o)
MAINSRC=ls.c
MAINOBJ=$(MAINSRC:.c=.o)
MAIN=ls
lib: $(OBJ)
	ar -rcs $(NAME) $(OBJ)
	ranlib $(NAME)
main:
	gcc -Wall -lbsd -L$(LIBPATH) $(MAINOBJ) $(LIBS) -o $(MAIN)
clean:
	-$(RM) *.o
	-$(RM) *~
	-$(RM) \#*
	-$(RM) *.core
	-$(RM) *.a
fclean: clean
	-$(RM) $(LIBPATH)/$(NAME)
.c.o:
	gcc $(CFLAGS) $< -o $@

all: clean lib main
