/**
	@file
	local_pool_test2.cpp.
*/


#include <iostream>

#include <boost/detail/local_pool.hpp>

using namespace std;
using namespace boost;


int main(void)
{
	local_pool pool[2];
	
	void * p = pool[0].malloc(10);
	
	cout << (pool[0].is_from(p) ? "true" : "false") << endl;
	cout << (pool[1].is_from(p) ? "true" : "false") << endl;

	return 0;
}
