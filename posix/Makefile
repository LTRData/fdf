all: fdf xorsum

install: all
	cp fdf xorsum /usr/local/bin/

fdf: fdf.o chkfile.o
	ld -o fdf fdf.o chkfile.o

fdf.o: fdf.cpp fdftable.hpp chkfile.h
	g++ -D_ALL_SOURCE -c fdf.cpp

chkfile.o: chkfile.c chkfile.h
	gcc -D_ALL_SOURCE -c chkfile.c

xorsum: xorsum.o chkfile.o
	ld -o xorsum xorsum.o chkfile.o

xorsum.o: xorsum.cpp chkfile.h
	g++ -D_ALL_SOURCE -c xorsum.cpp

clean:
	rm *.o *~
