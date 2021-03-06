.PHONY: all clean obj install

CXX = g++
CXXFLAGS = -g -Wall -Wextra -O2 -std=c++11 -DNDEBUG -Isrc -fPIC

SOURCES = $(wildcard src/*.cc)
MODULES := src/formats
-include $(patsubst %, %/module.mk, $(MODULES))
OBJECTS := $(patsubst %.cc, %.o, $(filter %.cc, $(SOURCES)))

TARGET = libkvzrtp.a

all: $(TARGET)

install: $(TARGET)
	install -m 577 $(TARGET) /usr/local/lib/
	mkdir -p /usr/local/include/kvzrtp /usr/local/include/kvzrtp/formats
	cp src/*.hh /usr/local/include/kvzrtp
	cp src/formats/*.hh /usr/local/include/kvzrtp/formats

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $(OBJECTS)

clean:
	rm -f src/*.o src/formats/*.o $(TARGET)
