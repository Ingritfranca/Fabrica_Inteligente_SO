CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra
TARGET   := fabrica

all: $(TARGET)

$(TARGET): main.cpp fabrica.cpp fabrica.hpp
	$(CXX) $(CXXFLAGS) -o $@ main.cpp fabrica.cpp

clean:
	rm -f $(TARGET) Robo_*.txt

