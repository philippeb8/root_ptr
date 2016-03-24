/*!
    \file
    \brief Simplest example of root_ptr.
    \details Includes code snippts for use in Quickbook documentation.

    Copyright 2016 Phil Bouchard.

    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt
    or copy at http://www.boost.org/LICENSE_1_0.txt

*/

//! [root_ptr_basic_1
#include <boost/smart_ptr/root_ptr.hpp>
using boost::root_ptr;
using boost::make_root;
//] [/root_ptr_basic_1]

#include <iostream>
struct A
{
    root_ptr<A> q;
//! [root_ptr_basic_2
    ~A()
    { // 
        std::cout << "~A()" << std::endl; // will get called.
    }
//! [/root_ptr_basic_2]
};

int main()
{
  //! [root_ptr_basic_3
   root_ptr<A> p = make_root<A>();
    p->q = p;

    p.reset(); // Deterministic destruction.
//] [/root_ptr_basic_3]
    return 0;
} //int main()

/*

Output:





*/

