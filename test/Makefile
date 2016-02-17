CXX             := g++
CXXFLAGS        := -O3 -std=c++11
INCPATH         := -I../include
LINK            := g++
LFLAGS          := -lboost_thread -lboost_system
RM              := rm
HEADERS         := $(wildcard *.h)
SOURCES         := $(wildcard *.cpp)
OBJECTS         := $(patsubst %.cpp,%.o,$(SOURCES))

.SUFFIXES: .cpp

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

.PHONY : all depend clean


all : block_ptr_test1 block_ptr_test3

block_ptr_test1: block_ptr_test1.o
	$(LINK) -o $@ $^ $(LFLAGS) -lboost_unit_test_framework

block_ptr_test3: block_ptr_test3.o
	$(LINK) -o $@ $^ $(LFLAGS) -lboost_unit_test_framework


Makefile.dep: $(SOURCES)
	$(CXX) ${INCPATH} -MM $^ > $@

clean:
	$(RM) -f block_ptr_test1 block_ptr_test3
	$(RM) -f $(OBJECTS)
	$(RM) -f *~ core

-include Makefile.dep
