CXX             := g++
CXXFLAGS        := -fpermissive -O3 -std=c++11
INCPATH         := -I../include -I/usr/local/include
LINK            := g++
LFLAGS          := -L/usr/local/lib -lboost_thread -lboost_system
RM              := rm
HEADERS         := $(wildcard *.h)
SOURCES         := $(wildcard *.cpp)
OBJECTS         := $(patsubst %.cpp,%.o,$(SOURCES))

.SUFFIXES: .cpp

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

.PHONY : all depend clean


all : benchmark root_ptr_example1 root_ptr_example2 root_ptr_example3 t100_test1 thread_test #allocator

benchmark: benchmark.o
	$(LINK) -o $@ $^ $(LFLAGS)

allocator: allocator.o
	$(LINK) -o $@ $^ $(LFLAGS)

root_ptr_example1: root_ptr_example1.o
	$(LINK) -o $@ $^ $(LFLAGS)

root_ptr_example2: root_ptr_example2.o
	$(LINK) -o $@ $^ $(LFLAGS)

root_ptr_example3: root_ptr_example3.o
	$(LINK) -o $@ $^ $(LFLAGS) -lboost_unit_test_framework

local_pool_test1: local_pool_test1.o
	$(LINK) -o $@ $^ $(LFLAGS)

local_pool_test2: local_pool_test2.o
	$(LINK) -o $@ $^ $(LFLAGS)

t100_test1: t100_test1.o
	$(LINK) -o $@ $^ $(LFLAGS) -lboost_regex

thread_test: thread_test.o
	$(LINK) -o $@ $^ $(LFLAGS)

Makefile.dep: $(SOURCES)
	$(CXX) ${INCPATH} -MM $^ > $@

clean:
	$(RM) -f benchmark allocator root_ptr_example1 root_ptr_example2 root_ptr_example3 local_pool_test1 local_pool_test2 t100_test1 thread_test
	$(RM) -f $(OBJECTS)
	$(RM) -f *~ core

-include Makefile.dep
