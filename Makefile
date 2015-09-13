
INSTALL?=install

OBJECTS=sfklCoding.o sfklDiff.o sfklLPC.o sfklZip.o sfklCrunch.o sfklFile.o sfklString.o

ENDIANNESS=LITTLE_ENDIAN

CXXFLAGS+=-fPIC -D__$(ENDIANNESS)__ -Wall -Wextra

OS := $(shell uname)
ifeq ($(OS),Darwin)
LDFLAGS += -flat_namespace -undefined suppress -dynamiclib
SO = dylib
else 
LDFLAGS += -shared
SO = so
INSTALL += -D
endif

all: libsfark.$(SO).0.0.0

clean:
	-rm *.o libsfark.$(SO).0.0.0

libsfark.$(SO).0.0.0: $(OBJECTS)
	$(CXX) -shared $(LDFLAGS) -Wl,-soname,libsfark.$(SO).0 $(OBJECTS) -o libsfark.$(SO).0.0.0

install: libsfark.$(SO).0.0.0 sfArkLib.h
	$(INSTALL) libsfark.$(SO).0.0.0 $(DESTDIR)/usr/lib/${DEB_HOST_MULTIARCH}/libsfark.$(SO).0.0.0
	$(INSTALL) sfArkLib.h $(DESTDIR)/usr/include/sfArkLib.h
	ln -s libsfark.$(SO).0.0.0 $(DESTDIR)/usr/lib/${DEB_HOST_MULTIARCH}/libsfark.$(SO).0
	ln -s libsfark.$(SO).0.0.0 $(DESTDIR)/usr/lib/${DEB_HOST_MULTIARCH}/libsfark.$(SO)
