INCS    =
LIBS    = -l curl

SRC_STATUSLIB = status.c
OBJ_STATUSLIB = ${SRC_STATUSLIB:.c=.o}
SRC_STATUSCLI = status_cli.c
OBJ_STATUSCLI = ${SRC_STATUSCLI:.c=.o}

CC      = gcc
CFLAGS  = -c -g -Wall -Wextra -Werror -pie -fPIC ${INCS}
LDFLAGS = ${LIBS}

all: libstatus.so status

.c.o: 
	@echo CC $<
	@${CC} ${CFLAGS} $<

libstatus.so:	${OBJ_STATUSLIB}
	@echo CC -o $@
	@${CC} -shared -o $@ ${OBJ_STATUSLIB} ${LDFLAGS}

status:	${OBJ_STATUSCLI}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ_STATUSCLI} -L ${CURDIR} -l status -Wl,-rpath,$(CURDIR)

clean:
	@echo cleaning
	@rm -f status ${OBJ_STATUSLIB} ${OBJ_STATUSCLI}
