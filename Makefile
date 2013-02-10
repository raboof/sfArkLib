all: libsfark.so

OBJECTS=sfklCoding.o sfklDiff.o sfklLPC.o sfklZip.o sfklCrunch.o sfklFile.o sfklString.o

ENDIANNESS=LITTLE_ENDIAN

CXXFLAGS+=-fPIC -D__$(ENDIANNESS)__ 

clean:
	-rm *.o libsfark.so

libsfark.so: $(OBJECTS)
	$(CXX) -shared $(OBJECTS) -o libsfark.so

install: sfArkLib.so sfArkLib.h
	cp libsfark.so $(DESTDIR)/usr/local/lib
	cp sfArkLib.h $(DESTDIR)/usr/local/include
