
CC = gcc
CXX = g++
CFLAGS = -O2 -Wall $(shell wx-config --cflags)
CXXFLAGS = -O2 -Wall $(shell wx-config --cxxflags)
LDFLAGS = 
LIBS = $(shell wx-config --libs std,aui)

OBJS = swoosh_app.o swoosh_frame.o swoosh_node.o swoosh_sender.o swoosh_data.o network.o util.o

all: swoosh

clean:
	rm -f *.o swoosh

swoosh: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<

