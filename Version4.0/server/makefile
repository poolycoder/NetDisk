CXX := g++
CXXFLAGS := -std=c++11 -D_GNU_SOURCE -Wall -Wextra -O2 -pthread $(shell mysql_config --cflags)
LDLIBS := $(shell mysql_config --libs) -lssl -lcrypto -pthread

TARGET := server
SOURCES := server.cpp tcp_init.cpp MyDb.cpp factory.cpp workQue.cpp \
	timer.cpp session.cpp ../common/config.cpp ../common/connection.cpp \
	../common/crypto.cpp ../common/protocol.cpp
OBJECTS := $(SOURCES:.cpp=.o)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -I../common -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: clean
