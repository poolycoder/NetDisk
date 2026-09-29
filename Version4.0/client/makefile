CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -O2 -pthread
LDLIBS := -lssl -lcrypto -pthread

TARGET := client
SOURCES := client.cpp upLoad_downLoad.cpp \
	../common/config.cpp ../common/connection.cpp \
	../common/crypto.cpp ../common/protocol.cpp
OBJECTS := $(SOURCES:.cpp=.o)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -I../common -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: clean
