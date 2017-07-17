Dependencies:

Flex, Bison++, Boost, Qt


Comparison with Node.JS:

$ qmake  
$ make  
$ ./xpp2cpp < tests/input1.xpp > tmp.cpp  
$ g++-5 -DBOOST_DISABLE_THREADS -std=c++11 -O2 -I./include -I../include tmp.cpp -otmp -lboost_system  
$ time ./tmp  
$ time js tests/input1.xpp  

JS2CPP is 2.7 times faster than Node.JS and there is no lag.
