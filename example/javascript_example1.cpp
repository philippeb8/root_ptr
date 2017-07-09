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
    char const * name;
    
    node_ptr<A> i;
    node_ptr<int> j;
    
    A(node_proxy const & x, char const * name): name(name), i(x), j(make_node<int>(x, 10)) 
    {
        cout << __PRETTY_FUNCTION__ << ": " << name << endl;         
    }
    
    ~A() 
    { 
        cout << __PRETTY_FUNCTION__ << ": " << name << endl; 
    }
};


// Metadata
namespace boost
{
template <>
    struct info_t<A>
    {
        static void proxy(A const & o, node_proxy const & x)
        {
            o.i.proxy(x);
            o.j.proxy(x);
        }
    };
}

int main()
{
    cout << "Scope 0: BEGIN" << endl;
    {
        node_proxy x; // 1st proxy
        node_ptr<A> a1 = make_node<A>(x, x, "a1");
        
        cout << "Scope 1: BEGIN" << endl;
        {
            node_proxy x; // 2nd proxy
            node_ptr<A> b1 = make_node<A>(x, x, "b1");
            node_ptr<A> b2 = make_node<A>(x, x, "b2");

            a1 = b1; // upscale scope of b1 to use 1st proxy
            
            b1 = make_node<A>(x, x, "b3"); // scope of b1 will still be associated with the 1st proxy
            b1->i = b1; // cycle
        }
        cout << "Scope 1: END" << endl;
    }
    cout << "Scope 0: END" << endl;
}
