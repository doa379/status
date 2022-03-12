INCS    = -I ..
LIBS    = -l sock

SRC_STATUSLIB = status.c
OBJ_STATUSLIB = $(SRC_STATUSLIB:.c=.o)
SRC_STATUSCLI = status_cli.c
OBJ_STATUSCLI = $(SRC_STATUSCLI:.c=.o)

CC          = cc
CC_FLAGS    = -Wall -Wextra -Werror -fPIE -fPIC 
REL_CFLAGS  = -O3
REL_LDFLAGS = -s
DBG_CFLAGS  = -g -O0
DBG_LDFLAGS =

CFLAGS  = $(REL_CFLAGS)
LDFLAGS = $(REL_LDFLAGS)

ifeq ($(DEBUG), 1)
  CFLAGS  = $(DBG_CFLAGS)
  LDFLAGS = $(DBG_LDFLAGS)
endif

all: libstatus.so status

.c.o: 
	@echo CC $<
	@${CC} $(CC_FLAGS) $(CFLAGS) $(INCS) -c $<

libstatus.so:	$(OBJ_STATUSLIB)
	@echo CC -o $@
	@$(CC) -shared -o $@ $(OBJ_STATUSLIB) $(LDFLAGS) $(LIBS)

status:	$(OBJ_STATUSCLI)
	@echo CC -o $@
	@$(CC) $(OBJ_STATUSCLI) -L $(CURDIR) -l status -Wl,-rpath,$(CURDIR) $(LDFLAGS) -o $@

clean:
	@echo Cleaning...
	@rm -f libstatus.so status $(OBJ_STATUSLIB) $(OBJ_STATUSCLI)
