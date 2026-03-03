CXX = g++
CXXFLAGS = -Wall -std=c++17

SRC_DIR = ..
TARGET = bronzedb

SRCS = $(SRC_DIR)/main.cpp
OBJS = $(notdir $(SRCS:.cpp=.o))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET)
