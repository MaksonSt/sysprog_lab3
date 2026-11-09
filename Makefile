CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Isrc -Ibuild
LDFLAGS  = -Wl,--stack,16777216
BUILD    = build
TARGET   = minicpp.exe

SRCS = $(wildcard src/*.cpp)
HDRS = $(wildcard src/*.hpp)
GEN  = $(BUILD)/parser.tab.cpp $(BUILD)/lexer.cpp

all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/parser.tab.cpp $(BUILD)/parser.tab.hpp: src/parser.y | $(BUILD)
	bison -d -v -Wcounterexamples -o $(BUILD)/parser.tab.cpp src/parser.y

$(BUILD)/lexer.cpp: src/lexer.l $(BUILD)/parser.tab.hpp | $(BUILD)
	flex -o $(BUILD)/lexer.cpp src/lexer.l

$(TARGET): $(GEN) $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(GEN) $(SRCS) $(LDFLAGS)

clean:
	rm -rf $(BUILD) $(TARGET)

.PHONY: all clean
