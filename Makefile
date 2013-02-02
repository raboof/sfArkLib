all: sfArkLib.so

clean:
	@rm *.o sfArkLib.so

OBJECTS=sfklCoding.o sfklDiff.o sfklLPC.o sfklZip.o sfklCrunch.o sfklFile.o sfklString.o

ENDIANNESS=LITTLE_ENDIAN

CXXFLAGS=-D__$(ENDIANNESS)__ 

sfArkLib.so: $(OBJECTS)
	$(CXX) -shared $(OBJECTS) -o sfArkLib.so
