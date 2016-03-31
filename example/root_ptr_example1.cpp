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

//[root_ptr_example1_include_1
#include <boost/smart_ptr/root_ptr.hpp>
//] [/root_ptr_example1_include_1]

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

//[root_ptr_example1_rvalue
root_ptr<int> foo()
{
    return make_root<int>(9);
}
//] [/root_ptr_example1_rvalue]


void bar(node_ptr<B> p)
{
    std::cout << p->i << std::endl;
}

int main()
{
  std::cout << "R-value:" << std::endl;
  {
//[root_ptr_example1_rvalue_call
    std::cout << * foo() << std::endl; 
//] [/root_ptr_example1_rvalue_call]
  }
  std::cout << std::endl;

  std::cout << "Slicing:" << std::endl;
  {
//[root_ptr_example1_slicing
    root_ptr<C> p = make_root<C>();
    bar(p);
//] [/root_ptr_example1_slicing]
  }
  std::cout << std::endl;

  
  std::cout << "Sharing:" << std::endl;
  {
//[root_ptr_example1_sharing
    root_ptr<int> p = make_root<int>(9);
    root_ptr<int> q = p;

    std::cout << "p: " << *p << std::endl;
    std::cout << "q: " << *q << std::endl;
 //] [//root_ptr_example1_sharing]
  }
  std::cout << std::endl;
  
  std::cout << "Cyclicism:" << std::endl;
  {
//[root_ptr_example1_cyclism

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

//] [/root_ptr_example1_cyclism]
  }
  std::cout << std::endl;

      // The following doesn't work with MSVC:
    // error C2676: binary '[': 'boost::root_ptr<char [9]>' does not define this operator or a conversion to a type acceptable to the predefined operator.
#if !defined(_MSC_VER)
    std::cout << "Array access:" << std::endl;
    {
        root_ptr<char[9]> u(new node<char[9]>());

        u[4] = 'Z';

        std::cout << "u[4] = " << u[4] << std::endl;
    }
    std::cout << std::endl;
#endif // !defined(_MSC_VER)

  std::cout << "Order of destruction:" << std::endl;
  {
    root_ptr<A> x;
    node_ptr<A> v = make_node<A>(x, x, 0);
    v->p = make_node<A>(x, x, 1);
    v->p->p = make_node<A>(x, x, 2);
    v->p->p->p = v->p;
  }
  std::cout << std::endl;

  { // advanced showing use of function cyclic
 //[root_ptr_example1_cyclic
    struct A
    {
      root_ptr<A> p;

      void foo() {}

      ~A()
      {
        if (!p.cyclic())
        {
          p->foo();
        }
      }
    };

    root_ptr<A> p;
    p = make_root<A>();
    p->p = p;
    //] [/root_ptr_example1_cyclic]
  }

  { // Advanced showing Propagating node_proxy information
//[root_ptr_example1_propagate
    struct A
    {
      node_ptr<A> p;

      A(node_proxy const & x) : p(x) {}
    };

    root_ptr<A> x;
    x = make_root<A>(x);
//] [/root_ptr_example1_propagate]
  }
} // int main()

/*

//[root_ptr_example1_output_rvalue
R-value:
9
//] //[/root_ptr_example1_output_rvalue]

//[root_ptr_example1_output_slicing
Slicing:
9
//] [/root_ptr_example1_output_slicing]

//[root_ptr_example1_output_sharing
Sharing:
p: 9
q: 9
//] //[/root_ptr_example1_output_sharing]

//[root_ptr_example1_output_cyclism
Cyclicism:
__thiscall A::A(const class boost::node_proxy &,int): 7
__thiscall A::A(const class boost::node_proxy &,int): 8
__thiscall A::A(const class boost::node_proxy &,int): 9
__thiscall A::~A(void): 8
p->i = 7
q->i = 9
r->i = 9
* v = 12
__thiscall A::~A(void): 9
__thiscall A::~A(void): 7
//] [/root_ptr_example1_output_cyclism]

//[root_ptr_example1_output_destruction
Order of destruction:
__thiscall A::A(const class boost::node_proxy &,int): 0
__thiscall A::A(const class boost::node_proxy &,int): 1
__thiscall A::A(const class boost::node_proxy &,int): 2
__thiscall A::~A(void): 0
__thiscall A::~A(void): 1
__thiscall A::~A(void): 2
//] [/root_ptr_example1_output_destruction]

*/
