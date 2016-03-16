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
    return new node<int>(9);
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
        root_ptr<C> p = new node<C>();
        bar(p);
    }
    cout << endl;

    cout << "Sharing:" << endl;
    {
        root_ptr<int> p = new node<int>(9);
        root_ptr<int> q = p;
        
        cout << "p: " << * p << endl;
        cout << "q: " << * q << endl;
    }
    cout << endl;

#if 1
    cout << "Cyclicism:" << endl;
    {
        root_ptr<A> x;
        node_ptr<A> p(x, new node<A>(x, 7));
        node_ptr<A> q(x, new node<A>(x, 8));
        node_ptr<A> r(x, new node<A>(x, 9));

        //node_ptr<void> t = make_node<A>(10);
        node_ptr<int volatile> v(x, new node<int volatile>(11));

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
        node_ptr<A> v(x, new node<A>(x, 0));
        v->p = new node<A>(x, 1);
        v->p->p = new node<A>(x, 2);
        v->p->p->p = v->p;
    }
    cout << endl;
}
