/**
	@file
	benchmark.cpp

	@note
	Copyright (c) 2011 Phil Bouchard <phil@fornux.com>.

	Distributed under the Boost Software License, Version 1.0.

	See accompanying file LICENSE_1_0.txt or copy at
	http://www.boost.org/LICENSE_1_0.txt

	See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/

#include <sys/time.h>
#include <sched.h>

#include <memory>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/block_ptr.hpp>

using namespace std;
using namespace boost;

template <typename T>
	auto_ptr<T> make_auto()
	{
		return auto_ptr<T>(new T);
	}

template <typename T, T (*P)()>
	void worker_make()  
	{         
		T p;

	    for (int i = 0; i < 100000; ++ i)
	    	p = P();
	}  
   
template <typename T, typename U>
	void worker_new()  
	{         
		T p;

	    for (int i = 0; i < 100000; ++ i)
	    	p.reset(new U);
	}  
   
timespec diff(timespec start, timespec end);

int main(int argc, char* argv[])  
{  
	timespec ts[2];

	const int n = 5;
	long median[n][3];
	
	for (int i = 0; i < n; ++ i)
	{
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, & ts[0]); 
		worker_make< auto_ptr<int>, make_auto<int> >();
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, & ts[1]);
		median[i][0] = diff(ts[0], ts[1]).tv_nsec;

		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, & ts[0]); 
		worker_make< shared_ptr<int>, make_shared<int> >();
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, & ts[1]);
		median[i][1] = diff(ts[0], ts[1]).tv_nsec;

		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, & ts[0]); 
		worker_make< block_ptr<int>, make_block<int> >();
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, & ts[1]);
		median[i][2] = diff(ts[0], ts[1]).tv_nsec;
	}
	
	cout << "make:" << endl;
	cout << "auto_ptr:\t" << setw(numeric_limits<long>::digits10 + 2) << median[n/2][0] << " ns" << endl;
	cout << "shared_ptr:\t" << setw(numeric_limits<long>::digits10 + 2) << median[n/2][1] << " ns" << endl;
	cout << "block_ptr:\t" << setw(numeric_limits<long>::digits10 + 2) << median[n/2][2] << " ns" << endl;
	cout << endl;
	
	for (int i = 0; i < n; ++ i)
	{
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, & ts[0]); 
		worker_new< auto_ptr<int>, int >();
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, & ts[1]);
		median[i][0] = diff(ts[0], ts[1]).tv_nsec;

		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, & ts[0]); 
		worker_new< shared_ptr<int>, int >();
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, & ts[1]);
		median[i][1] = diff(ts[0], ts[1]).tv_nsec;

		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, & ts[0]); 
		worker_new< block_ptr<int>, block<int> >();
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, & ts[1]);
		median[i][2] = diff(ts[0], ts[1]).tv_nsec;
	}

	cout << "new:" << endl;
	cout << "auto_ptr:\t" << setw(numeric_limits<long>::digits10 + 2) << median[n/2][0] << " ns" << endl;
	cout << "shared_ptr:\t" << setw(numeric_limits<long>::digits10 + 2) << median[n/2][1] << " ns" << endl;
	cout << "block_ptr:\t" << setw(numeric_limits<long>::digits10 + 2) << median[n/2][2] << " ns" << endl;
	cout << endl;
	
    return 0;  
}  

timespec diff(timespec start, timespec end)
{
	timespec temp;
	if ((end.tv_nsec-start.tv_nsec)<0) {
		temp.tv_sec = end.tv_sec-start.tv_sec-1;
		temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec-start.tv_sec;
		temp.tv_nsec = end.tv_nsec-start.tv_nsec;
	}
	return temp;
}
