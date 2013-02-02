all: sfArkLib.so

OBJECTS=sfklCoding.o sfklDiff.o sfklLPC.o sfklZip.o sfklCrunch.o sfklFile.o sfklString.o

ENDIANNESS=LITTLE_ENDIAN

CXXFLAGS=-D__$(ENDIANNESS)__ 

clean:
	@rm *.o sfArkLib.so

sfArkLib.so: $(OBJECTS)
	$(CXX) -shared $(OBJECTS) -o sfArkLib.so

install: sfArkLib.so sfArkLib.h
	cp sfArkLib.so $(DESTDIR)/usr/local/lib
	cp sfArkLib.h $(DESTDIR)/usr/local/include
