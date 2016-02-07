CXX             := g++
CXXFLAGS        := -O3
INCPATH         := -I../../.. -I../../../../../include
LINK            := g++
LFLAGS          := -L../../../../../lib -lboost_thread
RM              := rm
HEADERS         := $(wildcard *.h)
SOURCES         := $(wildcard *.cpp)
OBJECTS         := $(patsubst %.cpp,%.o,$(SOURCES))

.SUFFIXES: .cpp

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

.PHONY : all depend clean


all : block_ptr_test1 block_ptr_test2 block_ptr_test3 local_pool_test1 local_pool_test2 t100_test1

block_ptr_test1: block_ptr_test1.o
	$(LINK) $(LFLAGS) -o $@ $^ -lboost_unit_test_framework

block_ptr_test2: block_ptr_test2.o
	$(LINK) $(LFLAGS) -o $@ $^

block_ptr_test3: block_ptr_test3.o
	$(LINK) $(LFLAGS) -o $@ $^ -lboost_unit_test_framework

local_pool_test1: local_pool_test1.o
	$(LINK) $(LFLAGS) -o $@ $^

local_pool_test2: local_pool_test2.o
	$(LINK) $(LFLAGS) -o $@ $^

t100_test1: t100_test1.o
	$(LINK) $(LFLAGS) -o $@ $^ -lboost_regex


Makefile.dep: $(SOURCES)
	$(CXX) ${INCPATH} -MM $^ > $@

clean:
	$(RM) block_ptr_test1 block_ptr_test2 block_ptr_test3 local_pool_test1 local_pool_test2 t100_test1
	$(RM) $(OBJECTS)
	$(RM) *~ core

-include Makefile.dep
