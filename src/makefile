#Makefile for quicktrace
all:
	g++ -O3 -Wall -o qtrace testqt.cpp quicktrace.cpp

# To compile with _MSC_VER flag on unix
msc_ver_with_gcc:
	g++ -O3 -Wall -D _MSC_VER -D _MSC_VER_WITH_GCC -o qtrace testqt.cpp quicktrace.cpp

debug:
	g++ -g -Wall -o qtrace testqt.cpp quicktrace.cpp

clean:
	rm -f *.o qtrace