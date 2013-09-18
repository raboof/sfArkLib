all: libsfark.so

INSTALL?=install

OBJECTS=sfklCoding.o sfklDiff.o sfklLPC.o sfklZip.o sfklCrunch.o sfklFile.o sfklString.o

ENDIANNESS=LITTLE_ENDIAN

CXXFLAGS+=-fPIC -D__$(ENDIANNESS)__ -Wall -Wextra

clean:
	-rm *.o libsfark.so

libsfark.so: $(OBJECTS)
	$(CXX) -shared -dynamiclib $(OBJECTS) -o libsfark.so

install: libsfark.so sfArkLib.h
	$(INSTALL) -D libsfark.so $(DESTDIR)/usr/local/lib/libsfark.so
	$(INSTALL) -D sfArkLib.h $(DESTDIR)/usr/local/include/sfArkLib.h
