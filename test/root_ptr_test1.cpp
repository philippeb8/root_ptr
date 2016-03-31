/*!
\file
\brief
\details See http://www.boost.org/libs/root_ptr/doc/index.html for documentation.
*/

/*
Copyright Phil Bouchard, 2016

Distributed under the Boost Software License, Version 1.0.
See accompanying file LICENSE_1_0.txt
or copy at http://www.boost.org/LICENSE_1_0.txt
*/

#include <list>
#include <iostream>
#include <algorithm>

#include <boost/smart_ptr/root_ptr.hpp>
#include <boost/current_function.hpp>

//#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN
#define BOOST_LIB_DIAGNOSTIC "on" // Report library file details.

#include <boost/test/unit_test.hpp>


using namespace std;
using namespace boost;


struct A
{
    static int counter;
    static list<int> order;
    
    int i;
    node_ptr<A> p;
    
    A(node_proxy const & x) : i(counter ++) , p(x)
    {
    }
    
    ~A()
    {
        order.push_back(i);
    }
};

int A::counter = 0;
list<int> A::order;

BOOST_AUTO_TEST_CASE(root_ptr_test1)
{
    {
        root_ptr<A> x;
        node_ptr<A> p = make_node<A>(x, x); // 0
        node_ptr<A> q = make_node<A>(x, x); // 1
        node_ptr<A> r = make_node<A>(x, x); // 2

        root_ptr<A> t = make_root<A>(x); // 3

        p->p = p;
        q = r;

    }
    int order1[] = {1, 3, 2, 0};
    BOOST_CHECK( equal(A::order.begin(), A::order.end(), order1) );
    A::order.clear();

    // The following don't work with MSVC:
#if 0 //! defined(_MSC_VER)
    {
        root_ptr<A[5]> s = make_root<A[5]>(); // 4, 5, 6, 7, 8
    }
    int order2[] = {8, 7, 6, 5, 4};
    BOOST_CHECK( equal(A::order.begin(), A::order.end(), order2) );
    A::order.clear();
#endif

    {
        root_ptr<A> x;
        node_ptr<A> v = make_node<A>(x, x); // 4
        v->p = make_node<A>(x, x); // 5
        v->p->p = make_node<A>(x, x); // 6
        v->p->p->p = v;
    }
    int order3[] = {4, 5, 6};
    BOOST_CHECK( equal(A::order.begin(), A::order.end(), order3) );
    A::order.clear();
}
