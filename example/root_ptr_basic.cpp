/*!
  \file
  \brief Simplest example of root_ptr.
  \details Includes code snippets for use in Quickbook documentation.
*/
/*
  Copyright 2016 Phil Bouchard.

  Distributed under the Boost Software License, Version 1.0.
  See accompanying file LICENSE_1_0.txt
  or copy at http://www.boost.org/LICENSE_1_0.txt
*/

//[root_ptr_basic_0
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/weak_ptr.hpp>
using boost::shared_ptr;
using boost::weak_ptr;

#include <boost/make_shared.hpp>
using boost::make_shared;
//] [/root_ptr_basic_0]

//[root_ptr_basic_1
#include <boost/smart_ptr/root_ptr.hpp>
using boost::root_ptr;
using boost::node_ptr;
using boost::make_root;
using boost::node_proxy;

//] [/root_ptr_basic_1]

#include <iostream>

struct A
{
  node_ptr<A> q;
    
  A(node_proxy const & x) : q(x) {}
//[root_ptr_basic_A_noisy_death
  ~A()
  { // 
      std::cout << "~A()" << std::endl; // ~A() output when A is destructed.
  }
//] [/root_ptr_basic_A_noisy_death]
};

//[root_ptr_basic_S
struct S
{
  shared_ptr<S> q;
  ~S()
  { // 
    std::cout << "~S()" << std::endl; // ~S() output when S is destructed.
  }
};
//] [/root_ptr_basic_S]

//[root_ptr_basic_W
struct W
{
  weak_ptr<W> q;
  ~W()
  { // 
    std::cout << "~W()" << std::endl; // ~W() output when W is destructed.
  }
};
//] [/root_ptr_basic_W]

int main()
{
  std::cout << "A struct with a destructor that outputs when called.";
  {
    root_ptr<A> x;
    A a(x); // When A goes out of scope then its destructor outputs "~A()".
  }

  std::cout << "Using shared_ptr and reference counting." << std::endl;
  { // Using shared_ptr and reference counting.
 //[root_ptr_basic_shared

    shared_ptr<int> p = make_shared<int>(11);
    shared_ptr<int> q = p;

    p.reset();

    std::cout << *q << std::endl; // Outputs 11.
//] [/root_ptr_basic_shared]
  }

  std::cout << "Cyclic set using shared_ptr and reference counting.." << std::endl;

  { // Cyclic set using shared_ptr and reference counting.
//[root_ptr_basic_shared_cylic

      shared_ptr<S> p = make_shared<S>();
      p->q = p; // Create a cycle.

      p.reset(); // Detach from the cycle.
//] [/root_ptr_basic_shared_cyclic]
  }

  std::cout << "Cyclic set using weak_ptr." << std::endl;

  { // Cyclic set using weak_ptr.
//[root_ptr_basic_weak_cylic

    shared_ptr<W> p = make_shared<W>();
    p->q = p; // Create a cycle.

    p.reset(); // Detach from the cycle.
//] [/root_ptr_basic_weak_cyclic]
  }
  std::cout << "Use of root_ptr." << std::endl;
  {
    root_ptr<A> p;
    p = make_root<A>(p); // When A goes out of scope then its destructor outputs "~A()".
  }

  std::cout << "Cyclic set using root_ptr." << std::endl;
//[root_ptr_basic_3
  root_ptr<A> p;
  p = make_root<A>(p);
  p->q = p;

  p.reset(); // Detach from the cycle.
    // Deterministic destruction so destructor is actually called.
//] [/root_ptr_basic_3]

    return 0;
} //int main()

/*

Output:

//[root_ptr_example1_output_1

R-value:
9
//] [/root_ptr_example1_output_1]

//[root_ptr_example1_output_2

Slicing:
9
//] [/root_ptr_example1_output_2]

//[root_ptr_example1_output_3

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
//] [/root_ptr_example1_output_3]

//[root_ptr_example1_output_4

Order of destruction:
__thiscall A::A(const class boost::node_proxy &,int): 0
__thiscall A::A(const class boost::node_proxy &,int): 1
__thiscall A::A(const class boost::node_proxy &,int): 2
__thiscall A::~A(void): 0
__thiscall A::~A(void): 1
__thiscall A::~A(void): 2
//] [/root_ptr_example1_output_4]


*/

