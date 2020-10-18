INCS    =
LIBS    = -l curl

SRC     = status.c
OBJ     = ${SRC:.c=.o}

CC      = gcc
CFLAGS  = -c -g -Wall -Wextra -Werror -pie ${INCS}
LDFLAGS = ${LIBS}

all: status

.c.o: 
	@echo CC $<
	@${CC} ${CFLAGS} $<

status:	${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f status ${OBJ}
