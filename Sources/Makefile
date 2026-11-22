# macOS, Linux용 빌드 (Windows는 CMakeLists.txt 사용)
CXX ?= c++
CC ?= cc
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
CPPFLAGS += -Isrc -Ithird_party/miniz
BUILD = build

LIB_SRC = $(filter-out src/main.cpp,$(wildcard src/*.cpp))
LIB_OBJ = $(patsubst src/%.cpp,$(BUILD)/%.o,$(LIB_SRC)) $(BUILD)/miniz.o

all: bin/hanseek bin/hanseek_tests

$(BUILD)/%.o: src/%.cpp src/*.h | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD)/miniz.o: third_party/miniz/miniz.c | $(BUILD)
	$(CC) -O2 -c $< -o $@

bin/hanseek: $(LIB_OBJ) $(BUILD)/main.o | bin
	$(CXX) $^ -o $@

bin/hanseek_tests: $(LIB_OBJ) tests/test_main.cpp | bin
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/test_main.cpp $(LIB_OBJ) -o $@

test: bin/hanseek_tests
	./bin/hanseek_tests tests/fixtures

$(BUILD) bin:
	mkdir -p $@

clean:
	rm -rf $(BUILD) bin

.PHONY: all test clean
