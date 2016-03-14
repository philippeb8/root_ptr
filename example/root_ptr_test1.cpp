/**
    @file
    node_ptr_test2.cpp

    @note
    Copyright (c) 2008 Phil Bouchard <pbouchard8@gmail.com>.

    Distributed under the Boost Software License, Version 1.0.

    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt

    See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/

#include <iostream>

#include <boost/smart_ptr/root_ptr.hpp>
#include <boost/current_function.hpp>


using namespace std;
using namespace boost;


struct A
{
    int i;
    node_ptr<A> p;
    
    A(node_proxy const & x, int i = 0) : i(i), p(x)
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
        root_ptr<A> x;
        node_ptr<A> p = node_ptr<A>(x, new node<A>(x, 7));
        node_ptr<A> q = node_ptr<A>(x, new node<A>(x, 8));
        node_ptr<A> r = node_ptr<A>(x, new node<A>(x, 9));

        //node_ptr<void> t = make_node<A>(10);
        node_ptr<int volatile> v = node_ptr<int volatile>(x, new node<int volatile>(11));

        p->p = p->p;
        q = r;
        v = node_ptr<int volatile>(x, new node<int volatile>(12));

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
        root_ptr<char[9]> u = root_ptr<char[9]>(new node<char[9]>());

        u[4] = 'Z';

        cout << "u[4] = " << u[4] << endl;
    }
    cout << endl;
#endif
#endif

    cout << "Order of destruction:" << endl;
    {
        root_ptr<A> x;
        node_ptr<A> v = node_ptr<A>(x, new node<A>(x, 0));
        v->p = new node<A>(x, 1);
        v->p->p = new node<A>(x, 2);
        v->p->p->p = v->p;
    }
    cout << endl;
}
