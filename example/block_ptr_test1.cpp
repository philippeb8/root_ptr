/**
    @file
    block_ptr_test2.cpp

    @note
    Copyright (c) 2008 Phil Bouchard <pbouchard8@gmail.com>.

    Distributed under the Boost Software License, Version 1.0.

    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt

    See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/

#include <iostream>

#include <boost/smart_ptr/block_ptr.hpp>
#include <boost/current_function.hpp>


using namespace std;
using namespace boost;


struct A
{
    int i;
    block_ptr<A> p;
    
    A(block_proxy const & x, int i = 0) : i(i), p(x)
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

#if 1
    cout << "Cyclicism:" << endl;
    {
        proxy_ptr<A> x;
        block_ptr<A> p = block_ptr<A>(x, new block<A>(x, 7));
        block_ptr<A> q = block_ptr<A>(x, new block<A>(x, 8));
        block_ptr<A> r = block_ptr<A>(x, new block<A>(x, 9));

        //block_ptr<void> t = make_block<A>(10);
        block_ptr<int volatile> v = block_ptr<int volatile>(x, new block<int volatile>(11));

        p->p = p->p;
        q = r;
        v = block_ptr<int volatile>(x, new block<int volatile>(12));

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
        proxy_ptr<char[9]> u = proxy_ptr<char[9]>(new block<char[9]>());

        u[4] = 'Z';

        cout << "u[4] = " << u[4] << endl;
    }
    cout << endl;
#endif
#endif

    cout << "Order of destruction:" << endl;
    {
        proxy_ptr<A> x;
        block_ptr<A> v = block_ptr<A>(x, new block<A>(x, 0));
        v->p = new block<A>(x, 1);
        v->p->p = new block<A>(x, 2);
        v->p->p->p = v->p;
    }
    cout << endl;
}
