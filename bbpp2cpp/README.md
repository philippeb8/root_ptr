Backbone++ Programming Language


Dependencies:

Flex, Bison++, Boost, Qt


Comparison with Node.JS:

$ qmake  
$ make  
$ ./bbpp2cpp < tests/input1.bb > tmp.cpp  
$ g++-5 -DBOOST_DISABLE_THREADS -std=c++11 -O2 -I./include -I../include tmp.cpp -otmp -lboost_system  
$ time ./tmp  
$ time js tests/input1.js  

BBPP2CPP is 3 times faster than Node.JS and there is no lag.
