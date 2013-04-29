CFLAGS=-Wall -O3 -g
CXXFLAGS=-Wall -O3 -g
OBJECTS=main.o gpio.o led-matrix.o thread.o
BINARIES=led-matrix
LDFLAGS=-lrt -lm -lpthread

all : led-matrix

led-matrix : $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

led-matrix.o : led-matrix.cc led-matrix.h

clean:
	rm -f $(OBJECTS) $(BINARIES)
