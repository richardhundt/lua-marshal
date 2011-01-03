# makefile for marshal library for Lua

# change these to reflect your Lua installation
LUA= /usr/local/bin/lua
LUAINC= /usr/local/include
LUALIB= $(LUA)
LUABIN= /usr/local/bin

# probably no need to change anything below here
CFLAGS= $(INCS) $(WARN) -O3 $G -fPIC
WARN= -ansi -pedantic -Wall
INCS= -I$(LUAINC)

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
	$(CC) -o $@ -shared $(OBJS)

clean:
	rm -f $(OBJS) $T

doc:
	@echo "$(MYNAME) library:"
	@fgrep '/**' $(MYLIB).c | cut -f2 -d/ | tr -d '*' | sort | column

