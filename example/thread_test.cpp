/**
	@file
	thread_test.cpp

	@note
	Copyright (c) 2011 Phil Bouchard <phil@fornux.com>.

	Distributed under the Boost Software License, Version 1.0.

	See accompanying file LICENSE_1_0.txt or copy at
	http://www.boost.org/LICENSE_1_0.txt

	See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/

#include <iostream>
#include <boost/thread.hpp>
#include <boost/block_ptr.hpp>
//#include <boost/thread/mutex.hpp>

using namespace std;
using namespace boost;


block_ptr< pair<int, int> > p;


void worker(int id)  
{         
    std::cout << "Worker: running" << std::endl;  
       
    for (int i = 0; i < 100000; ++ i)
    {
       	cout << id << "-" << i << ", " << flush;
    	p = new block< pair<int, int> >(make_pair(id, i));
    }
    cout << endl;
       
    std::cout << "Worker: finished" << std::endl;  
}  
   
int main(int argc, char* argv[])  
{  
    std::cout << "main: startup" << std::endl;  
    
   	boost::thread t0(worker, 0);  
   	boost::thread t1(worker, 1);  
   	boost::thread t2(worker, 2);  
   	boost::thread t3(worker, 3);  
       
    std::cout << "main: waiting for thread" << std::endl;  
       
    t0.join();  
    t1.join();  
    t2.join();  
    t3.join();  
       
    std::cout << "main: done" << std::endl;  
       
    return 0;  
}  
