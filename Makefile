CC=g++
CCFLAGS=
LDIR=/usr/local/lib
IDIR=/usr/local/include
EXECS=sandbox
OUTFILE=sandbox
DEBUG=
LUA:=
PATHLUA=$(dir $(realpath $(shell find /usr/include -name lua.hpp)))
ifneq (,$(wildcard $(PATHLUA)))
INCLUA=-I $(PATHLUA)
endif
ifndef INCLUA
  ifneq (,$(wildcard ./libs/lua/))
  INCLUA=-I ./libs/lua/
  endif
endif
ifneq (,$(wildcard ./libs/pcre/))
INCPCRE=-I ./libs/pcre/
endif
ifneq (,$(wildcard ./libs/boost/))
INCBOOST=-I ./libs/
endif

ifneq (,$(wildcard ./libs/lua/$(OS)$(ARCP)/))
LIBLUA?=-L ./libs/lua/$(OS)$(ARCP)/
endif
ifneq (,$(wildcard ./libs/pcre/$(OS)$(ARCP)/))
LIBPCRE?=-L ./libs/pcre/$(OS)$(ARCP)/
endif
ifneq (,$(wildcard ./libs/boost/$(OS)$(ARCP)/))
LIBBOOST?=-L ./libs/boost/$(OS)$(ARCP)/
endif

.PHONY: 32bit

OS := $(shell uname)
ifeq ($(OS), Darwin)
	MAC_MIN=-mmacosx-version-min=10.6
endif

all: $(EXECS)

32bit: ARCH=-arch i386 -m32
32bit: ARCP:=/x86
32bit: OUTFILE=sandbox32
32bit: sandbox


sandbox: engine.cpp tinycon.cpp vm.cpp md5.cpp
	@echo "Compiling Pluto (The Hacker's Sandbox)..."
	@echo "#########################################"
	$(CC) $(DEBUG) $(ARCH) $(MAC_MIN) -o $(OUTFILE) $(CCFLAGS) $^ $(INCLUA) $(LIBLUA) $(INCPCRE) $(LIBPCRE) $(INCBOOST) $(LIBBOOST) -llua$(LUA) -lpcre -lpcrecpp -lboost_thread -lboost_system
	@echo "done."
	@echo

clean:
	rm -rf *.o *.dSYM $(OUTFILE)
