CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra

SRCS = main.c++
OBJS = $(SRCS:.c++=.o)
EXEC = echo-client

.PHONY: all clean

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

%.o: %.c++
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(EXEC)
