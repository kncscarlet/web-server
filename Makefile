OPTIMIZATION := -O2
CXXFLAGS := -g -std=c++17 -Wno-unused-parameter -Wall -Wextra $(OPTIMIZATION)
LDLIBS := -lpthread -luring
LDFLAGS := -g $(OPTIMIZATION)
CC := $(CXX)

all: main

main.o: main.cpp main.hpp
io_uring.o: io_uring.cpp main.hpp
epoll.o: epoll.cpp main.hpp
main: main.o io_uring.o epoll.o

clean:
	@rm -f main *.o

.PHONY: all clean
