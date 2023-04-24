
CC = gcc
CXX = g++
#CFLAGS = -g -Og -Wall $(shell wx-config --cflags) -fsanitize=address,undefined
#CXXFLAGS = -g -Og -Wall $(shell wx-config --cxxflags) -fsanitize=address,undefined
#LDFLAGS = -g -fsanitize=address,undefined
CFLAGS = -g -Og -Wall $(shell wx-config --cflags)
CXXFLAGS = -g -Og -Wall $(shell wx-config --cxxflags)
LDFLAGS = -g
LIBS = $(shell wx-config --libs std,aui)

OBJS = swoosh_app.o swoosh_frame.o swoosh_node.o \
       swoosh_local_data.o swoosh_remote_data.o \
	   swoosh_data_store.o network.o util.o

all: swoosh

clean:
	rm -f *.o swoosh

swoosh: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<

