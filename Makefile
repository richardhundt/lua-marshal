# makefile for marshal library for Lua

# change these to reflect your Lua installation
LUA= /usr/local/bin/lua
LUAINC= /usr/local/include
LUALIB= $(LUA)
LUABIN= /usr/local/bin

WARN= -ansi -pedantic -Wall
INCS= -I$(LUAINC)

CFLAGS=-O3 $(INCS)
LDFLAGS=

OS_NAME=$(shell uname -s)
MH_NAME=$(shell uname -m)

ifeq ($(OS_NAME), Darwin)
CFLAGS+=-bundle -undefined dynamic_lookup
else ifeq ($(OS_NAME), Linux)
CFLAGS+=-shared -fPIC
endif

MYNAME= marshal
MYLIB= l$(MYNAME)
T= $(MYNAME).so
OBJS= $(MYLIB).o
TEST= test.lua

all:	test

test:	$T
	$(LUABIN)/lua $(TEST)

o:	$(MYLIB).o

so:	$T

$T:	$(OBJS)
	$(CC) $(CFLAGS) $(WARN) -o $@ $(OBJS)

clean:
	rm -f $(OBJS) $T

doc:
	@echo "$(MYNAME) library:"
	@fgrep '/**' $(MYLIB).c | cut -f2 -d/ | tr -d '*' | sort | column

