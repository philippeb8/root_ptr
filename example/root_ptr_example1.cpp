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

struct B
{
    int i;
    
    B() : i(9) {}
};

struct C : B
{
};

root_ptr<int> foo()
{
    return make_root<int>(9);
}

void bar(node_ptr<B> p)
{
    cout << p->i << endl;
}

int main()
{
    cout << "R-value:" << endl;
    {
        cout << * foo() << endl;
    }
    cout << endl;

    cout << "Slicing:" << endl;
    {
        root_ptr<C> p = make_root<C>();
        bar(p);
    }
    cout << endl;
	
#if 0
    cout << "Sharing:" << endl;
    {
        root_ptr<int> p = make_root<int>(9);
        root_ptr<int> q = p;
        
        cout << "p: " << * p << endl;
        cout << "q: " << * q << endl;
    }
    cout << endl;
#endif

#if 1
    cout << "Cyclicism:" << endl;
    {
        root_ptr<A> x;
        node_ptr<A> p = make_node<A>(x, x, 7);
        node_ptr<A> q = make_node<A>(x, x, 8);
        node_ptr<A> r = make_node<A>(x, x, 9);

        //node_ptr<void> t = make_node<A>(10);
        node_ptr<int volatile> v = make_node<int volatile>(x, 11);

        p->p = p->p;
        q = r;
        v = make_node<int volatile>(x, 12);

        cout << "p->i = " << p->i << endl;
        cout << "q->i = " << q->i << endl;
        cout << "r->i = " << r->i << endl;
        cout << "* v = " << * v << endl;
    }
    cout << endl;

    // The following don't work with MSVC:
#if 0 //! defined(_MSC_VER)
    cout << "Array access:" << endl;
    {
        root_ptr<char[9]> u(new node<char[9]>());

        u[4] = 'Z';

        cout << "u[4] = " << u[4] << endl;
    }
    cout << endl;
#endif
#endif

    cout << "Order of destruction:" << endl;
    {
        root_ptr<A> x;
        node_ptr<A> v = make_node<A>(x, x, 0);
        v->p = make_node<A>(x, x, 1);
        v->p->p = make_node<A>(x, x, 2);
        v->p->p->p = v->p;
    }
    cout << endl;
}
