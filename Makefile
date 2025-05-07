CC=g++
CCFLAGS=-std=c++11
LDIR=/usr/local/lib
IDIR=/usr/local/include
EXECS=sandbox
OUTFILE=sandbox
DEBUG=

# Use system libraries - check /usr/local first, then fall back to /opt/homebrew
INCLUA=-I /usr/local/include/lua -I /opt/homebrew/include/lua
INCPCRE=-I /usr/local/include -I /opt/homebrew/include
INCBOOST=-I /usr/local/include -I /opt/homebrew/include

# Use system library paths - check /usr/local first, then fall back to /opt/homebrew
LIBLUA=-L /usr/local/lib -L /opt/homebrew/lib
LIBPCRE=-L /usr/local/lib -L /opt/homebrew/lib
LIBBOOST=-L /usr/local/lib -L /opt/homebrew/lib

.PHONY: 32bit

OS := $(shell uname)
ifeq ($(OS), Darwin)
	MAC_MIN=-mmacosx-version-min=10.9
	CCFLAGS+=-stdlib=libc++
	# Force static linking on macOS
	STATIC_LUA=-Wl,-force_load,/opt/homebrew/lib/liblua.a
	STATIC_PCRE=-Wl,-force_load,/opt/homebrew/lib/libpcre.a -Wl,-force_load,/opt/homebrew/lib/libpcrecpp.a
	STATIC_BOOST=-Wl,-force_load,/opt/homebrew/lib/libboost_thread.a -Wl,-force_load,/opt/homebrew/lib/libboost_system.a
else
	STATIC_FLAGS=-static-libstdc++ -static-libgcc
	STATIC_LUA=-llua
	STATIC_PCRE=-lpcre -lpcrecpp
	STATIC_BOOST=-lboost_thread -lboost_system
endif

all: $(EXECS)

32bit: ARCH=-arch i386 -m32
32bit: ARCP:=/x86
32bit: OUTFILE=sandbox32
32bit: sandbox

sandbox: engine.cpp tinycon.cpp vm.cpp md5.cpp
	@echo "Compiling Pluto (The Hacker's Sandbox)..."
	@echo "#########################################"
	$(CC) $(DEBUG) $(ARCH) $(MAC_MIN) -o $(OUTFILE) $(CCFLAGS) $(STATIC_FLAGS) $^ $(INCLUA) $(LIBLUA) $(INCPCRE) $(LIBPCRE) $(INCBOOST) $(LIBBOOST) $(STATIC_LUA) $(STATIC_PCRE) $(STATIC_BOOST)
	@echo "done."
	@echo

clean:
	rm -rf *.o *.dSYM $(OUTFILE)
