BB++ Programming Language


Dependencies:

Flex, Bison++, Boost, Qt


Comparison with Node.JS:

$ qmake  
$ make  
$ ./bbpp2cpp < tests/input3.bb > tmp.cpp  
$ g++-5 -DBOOST_DISABLE_THREADS -std=c++14 -I./include -I../include tmp.cpp -otmp -lboost_system -O3  
$ time ./tmp  
$ time js tests/input3.js  

BBPP2CPP is 4 times faster than Node.JS, there is no lag and the errors are reported at compile-time.


Here's a 5 minutes long documentary:  
https://youtu.be/vXmddU_FS30  
