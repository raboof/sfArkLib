
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

all: libsfark.$(SO)

clean:
	-rm *.o libsfark.$(SO)

libsfark.$(SO): $(OBJECTS)
	$(CXX) -shared $(LDFLAGS) $(OBJECTS) -o libsfark.$(SO)

install: libsfark.$(SO) sfArkLib.h
	$(INSTALL) libsfark.$(SO) $(DESTDIR)/usr/lib/libsfark.$(SO)
	$(INSTALL) sfArkLib.h $(DESTDIR)/usr/include/sfArkLib.h
