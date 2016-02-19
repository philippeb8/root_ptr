CXX             := g++-4.8
CXXFLAGS        := -O3 -DBOOST_DISABLE_THREADS -std=c++11
INCPATH         := -I../include
LINK            := g++-4.8
LFLAGS          := -lboost_thread -lboost_system
RM              := rm
HEADERS         := $(wildcard *.h)
SOURCES         := $(wildcard *.cpp)
OBJECTS         := $(patsubst %.cpp,%.o,$(SOURCES))

.SUFFIXES: .cpp

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

.PHONY : all depend clean


all : benchmark block_ptr_test1 block_ptr_test2 block_ptr_test3 t100_test1

benchmark: benchmark.o
	$(LINK) -o $@ $^ $(LFLAGS)

block_ptr_test1: block_ptr_test1.o
	$(LINK) -o $@ $^ $(LFLAGS)

block_ptr_test2: block_ptr_test2.o
	$(LINK) -o $@ $^ $(LFLAGS)

block_ptr_test3: block_ptr_test3.o
	$(LINK) -o $@ $^ $(LFLAGS) -lboost_unit_test_framework

local_pool_test1: local_pool_test1.o
	$(LINK) -o $@ $^ $(LFLAGS)

local_pool_test2: local_pool_test2.o
	$(LINK) -o $@ $^ $(LFLAGS)

t100_test1: t100_test1.o
	$(LINK) -o $@ $^ $(LFLAGS) -lboost_regex


Makefile.dep: $(SOURCES)
	$(CXX) ${INCPATH} -MM $^ > $@

clean:
	$(RM) -f benchmark block_ptr_test1 block_ptr_test2 block_ptr_test3 local_pool_test1 local_pool_test2 t100_test1
	$(RM) -f $(OBJECTS)
	$(RM) -f *~ core

-include Makefile.dep
