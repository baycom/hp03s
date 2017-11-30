PACKAGES=libcurl

CXX=g++

PROFILING=
DEFINES=
PKG_INCLUDES= $(shell pkg-config --cflags $(PACKAGES))
PKG_LIBS= $(shell pkg-config --libs $(PACKAGES))

CXXFLAGS=-O3 $(PROFILING) -mfloat-abi=hard -ftree-vectorize -ffast-math \
	      -std=c++0x -Wall \
	      -Wno-unused-function  -Wno-unused-variable -Wno-unused-but-set-variable \
	      -Wno-write-strings

INCLUDES= $(PKG_INCLUDES) 

LIBS=-lm -lwiringPi $(PKG_LIBS) 
LDFLAGS=$(PROFILING) -rdynamic

CFILES=hp03s.cpp

OBJS = $(CFILES:.cpp=.o)

all: hp03s

hp03s: $(OBJS)
	 $(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
#	 sync

.cpp.o:
	$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -c $<

ifneq ($(MAKECMDGOALS),clean)
include .depend
endif

.depend: Makefile
	touch .depend
	makedepend $(DEF) $(INCL)  $(CFLAGS) $(CFILES) -f .depend >/dev/null 2>&1 || /bin/true

clean :
	$(RM) $(OBJS) *~
	$(RM) hp03s
	$(RM) .depend
