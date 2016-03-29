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


#include <iostream>

//[block_ptr_example1_include_1
#include <boost/smart_ptr/root_ptr.hpp>
//] [/block_ptr_example1_include_1]

#include <boost/current_function.hpp>

using namespace boost;

struct A
{
    int i;
    node_ptr<A> p;

    A(node_proxy const & x, int i = 0) : i(i), p(x)
    {
        std::cout << BOOST_CURRENT_FUNCTION << ": " << i << std::endl;
    }

    ~A()
    {
        std::cout << BOOST_CURRENT_FUNCTION << ": " << i << std::endl;
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
    std::cout << p->i << std::endl;
}

int main()
{
    std::cout << "R-value:" << std::endl;
    {
        std::cout << * foo() << std::endl;
    }
    std::cout << std::endl;

    std::cout << "Slicing:" << std::endl;
    {
        root_ptr<C> p = make_root<C>();
        bar(p);
    }
    std::cout << std::endl;

#if 1
    {
      std::cout << "Sharing:" << std::endl;
      {
        root_ptr<int> p = make_root<int>(9);
        root_ptr<int> q = p;

        std::cout << "p: " << *p << std::endl;
        std::cout << "q: " << *q << std::endl;
      }
      std::cout << std::endl;
    }
#endif
 
#if 1
    std::cout << "Cyclicism:" << std::endl;
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

        std::cout << "p->i = " << p->i << std::endl;
        std::cout << "q->i = " << q->i << std::endl;
        std::cout << "r->i = " << r->i << std::endl;
        std::cout << "* v = " << * v << std::endl;
    }
    std::cout << std::endl;

    // The following don't work with MSVC:
#if 0 //! defined(_MSC_VER)
    std::cout << "Array access:" << std::endl;
    {
        root_ptr<char[9]> u(new node<char[9]>());

        u[4] = 'Z';

        std::cout << "u[4] = " << u[4] << std::endl;
    }
    std::cout << std::endl;
#endif
#endif

    std::cout << "Order of destruction:" << std::endl;
    {
        root_ptr<A> x;
        node_ptr<A> v = make_node<A>(x, x, 0);
        v->p = make_node<A>(x, x, 1);
        v->p->p = make_node<A>(x, x, 2);
        v->p->p->p = v->p;
    }
    std::cout << std::endl;
}

/*

//[root_ptr_example1_output1

Cyclicism:
 __thiscall A::A(int): 7
 __thiscall A::A(int): 8
 __thiscall A::A(int): 9
 __thiscall A::~A(void): 8

//] [/root_ptr_example1_output1]


 p->i = 7
 q->i = 9
 r->i = 9
 * v = 12
 __thiscall A::~A(void): 9
 __thiscall A::~A(void): 7

 //[root_ptr_example1_output2

 Order of destruction:
 __thiscall A::A(int): 0
 __thiscall A::A(int): 1
 __thiscall A::A(int): 2
 __thiscall A::~A(void): 0
 __thiscall A::~A(void): 1
 __thiscall A::~A(void): 2

 //] [/root_ptr_example1_output2]


*/
