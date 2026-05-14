CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra
TARGET   := fabrica

.PHONY: all clean run

all: $(TARGET)

$(TARGET): main.cpp fabrica.cpp fabrica.hpp
	$(CXX) $(CXXFLAGS) -o $@ main.cpp fabrica.cpp

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET) Robo_*.txt
