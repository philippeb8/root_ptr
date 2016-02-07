/**
	@file
	block_ptr_test2.cpp

	@note
	Copyright (c) 2008 Phil Bouchard <phil@fornux.com>.

	Distributed under the Boost Software License, Version 1.0.

	See accompanying file LICENSE_1_0.txt or copy at
	http://www.boost.org/LICENSE_1_0.txt

	See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/

#include <iostream>

#include <boost/block_ptr.hpp>
#include <boost/current_function.hpp>


using namespace std;
using namespace boost;


struct A
{
	int i;
	block_ptr<A> p;
	
	A(int i = 0) : i(i) 
	{
		cout << BOOST_CURRENT_FUNCTION << ": " << i << endl;
	}
	
	~A()
	{
		cout << BOOST_CURRENT_FUNCTION << ": " << i << endl;
	}
};


int main()
{
	cout << "Cyclicism:" << endl;
	{
		block_ptr<A> p = make_block<A>(7);
		block_ptr<A> q = make_block<A>(8);
		block_ptr<A> r = make_block<A>(9);

		block_ptr<void> t = make_block<A>(10);
		block_ptr<int const volatile> v = make_block<int const volatile>(11);

		p->p = p;
		q = r;
		v = make_block<int const volatile>(12);

		cout << "p->i = " << p->i << endl;
		cout << "q->i = " << q->i << endl;
		cout << "r->i = " << r->i << endl;
		cout << "* v = " << * v << endl;
	}
	cout << endl;

	// The following don't work with MSVC:
#if ! defined(_MSC_VER)
	cout << "Array access:" << endl;
	{
		block_ptr<A[5]> s = make_block<A[5]>();
		block_ptr<char[9]> u = make_block<char[9]>();

		u[4] = 'Z';

		cout << "u[4] = " << u[4] << endl;
	}
	cout << endl;
#endif

	cout << "Order of destruction:" << endl;
	{
		block_ptr<A> v = make_block<A>(0);
		v->p = make_block<A>(1);
		v->p->p = make_block<A>(2);
		v->p->p->p = v;
	}
	cout << endl;
}
