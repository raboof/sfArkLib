INSTALL?=install
USE_SOLINK:=

OBJECTS=sfklCoding.o sfklDiff.o sfklLPC.o sfklZip.o sfklCrunch.o sfklFile.o sfklString.o

ENDIANNESS=LITTLE_ENDIAN

CXXFLAGS+=-fPIC -D__$(ENDIANNESS)__ -Wall -Wextra

OS := $(shell uname)
ifeq ($(OS),Darwin)
LDFLAGS += -flat_namespace -undefined suppress -dynamiclib
SO = dylib
else 
LDFLAGS += -shared
SHLIB_MAJOR:=1
SHLIB_MINOR:=0
SHLIB_PATCHLEVEL:=0
ifneq (,$(filter Linux GNU/kFreeBSD GNU,${OS}))
INSTALL += -D
USE_SOLINK:=so.${SHLIB_MAJOR} so
SO = so.${SHLIB_MAJOR}.${SHLIB_MINOR}.${SHLIB_PATCHLEVEL}
LDFLAGS += -Wl,-soname,libsfark.so.${SHLIB_MAJOR}
else ifneq (,$(findstring BSD,${OS}))
# OpenBSD/MirBSD, might apply to NetBSD, FreeBSD/MidnightBSD might differ
SO = so.${SHLIB_MAJOR}.${SHLIB_MINOR}
else
SO = so
endif
endif

PREFIX ?= /usr/local
ifdef DEB_HOST_MULTIARCH
LIBDIR = ${PREFIX}/lib/${DEB_HOST_MULTIARCH}
else
LIBDIR = ${PREFIX}/lib
endif
INCDIR = ${PREFIX}/include

all: libsfark.$(SO)

.PHONY: clean

clean:
	-rm *.o libsfark.$(SO)

test: libsfark.$(SO)
	-rm -rf sfarkxtc
	git clone https://github.com/raboof/sfarkxtc
	CXXFLAGS="-I.. -L.." make -C sfarkxtc
	rm -rf sfarkxtc

libsfark.$(SO): $(OBJECTS)
	$(CXX) -shared $(LDFLAGS) $(OBJECTS) -o libsfark.$(SO) -lz

# It is unclear to me whether /usr/local/* is the proper location on
# OSX, as reportedly it's not on the clang path by default there. Let
# me know if you know how this should be done there :).
install: libsfark.$(SO) sfArkLib.h
	$(INSTALL) libsfark.$(SO) ${DESTDIR}${LIBDIR}/libsfark.$(SO)
	$(INSTALL) sfArkLib.h ${DESTDIR}${INCDIR}/sfArkLib.h
ifneq (,$(strip ${USE_SOLINK}))
	set -ex; for solink in ${USE_SOLINK}; do \
		rm -f ${DESTDIR}${LIBDIR}/libsfark.$$solink; \
		ln -s libsfark.$(SO) ${DESTDIR}${LIBDIR}/libsfark.$$solink; \
	done
endif
