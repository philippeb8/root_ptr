/*!
  \file
  \brief Example 2 of using root_ptr.
*/
/*
  Copyright Phil Bouchard 2017
  Distributed under the Boost Software License, Version 1.0.
  See accompanying file LICENSE_1_0.txt or copy at
  http://www.boost.org/LICENSE_1_0.txt

  See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#include <boost/smart_ptr/root_ptr.hpp>
#include <boost/mpl/range_c.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/array.hpp>
#include <boost/container/list.hpp>

#include <list>
#include <vector>
#include <iostream>

using namespace std;
using namespace boost;


// Example
struct A
{
    node_ptr<int> i;
    node_ptr<int> j;
    
    A(node_proxy const & x): i(x), j(x) 
    {
        cout << __PRETTY_FUNCTION__ << endl;         
    }
    
    ~A() 
    { 
        cout << __PRETTY_FUNCTION__ << endl; 
    }
};


// Metadata
namespace boost
{
template <>
    struct info_t<A>
    {
        static void proxy(A * po, node_proxy const * px)
        {
            po->i.proxy(px);
            po->j.proxy(px);
        }
    };
}

int main()
{
    cout << "Scope 0: BEGIN" << endl;
    {
        node_proxy x;
        node_ptr<A> a(x);
        
        cout << "Scope 1: BEGIN" << endl;
        {
            node_proxy x;
            node_ptr<A> b = make_node<A>(x, x);
            
            a = b;
            
            b = make_node<A>(x, x);
        }
        cout << "Scope 1: END" << endl;
    }
    cout << "Scope 0: END" << endl;
}
