/**
    @file
    block_ptr_test2.cpp

    @note
    Copyright (c) 2008 Steven Watanabe <watanabesj@gmail.com>

    Distributed under the Boost Software License, Version 1.0.

    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt

    See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#include <boost/smart_ptr/block_ptr.hpp>

#include <list>
#include <vector>
#include <iostream>

#include <boost/mpl/range_c.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/array.hpp>
#include <boost/container/list.hpp>


static int count;

using namespace boost;

struct node {
    node() {
        ++count;
    }
    ~node() {
        --count;
    }
    block_ptr<node> prior;
    block_ptr<node> next;
};

struct list {
public:
    list() {}
    void clear() {
        front.reset();
        back.reset();
    }
    void insert() {
        if(front.get() == 0) {
            back = new block<node>();
        } else {
            back->next = new block<node>();
            back->next->prior = back;
            back = back->next;
        }
    }
    ~list()
    {
    }
private:
    block_ptr<node> front;
    block_ptr<node> back;
};

struct vector {
    vector() { ++count; std::cout << __FUNCTION__ << "(): " << this << std::endl; }
    ~vector() { --count; std::cout << __FUNCTION__ << "(): " << this << std::endl; }
    vector(const vector& other) : elements(other.elements) { ++count; }
    //std::vector<block_ptr<vector> > elements;
    //std::list<block_ptr<vector> > elements;
    boost::container::list<block_ptr<vector> > elements; //! works fine
};

struct create_type {
    template<class T>
    void operator()(T) const {
        make_block<boost::array<char, T::value> >();
    }
};

int main() {
#if 1
    std::cout << "*** Test #1 ***" << std::endl;
    count = 0;
    {
        list l;
        for(int j = 0; j < 2; ++j) {
            for(int i = 0; i < 1000; ++i) {
                l.insert();
            }
            l.clear();
        }
    }
    std::cout << count << std::endl;
#endif
#if 1
    std::cout << "*** Test #2 ***" << std::endl;
    count = 0;
    {
        block_ptr<node> v = make_block<node>();
        v->next = v;
    }
    std::cout << count << std::endl;
#endif
#if 1
    std::cout << "*** Test #3 ***" << std::endl;
    count = 0;
    {
        block_ptr<vector> v = make_block<vector>();
        v->elements.push_back(make_block<vector>());
        v->elements.push_back(make_block<vector>());
        v->elements.push_back(v->elements.back());
        v->elements.push_back(v);
    }
    std::cout << count << std::endl;

    count = 0;
    {
        block_ptr<vector> v = make_block<vector>();
        v->elements.push_back(v);
    }
    std::cout << count << std::endl;

#endif
#if 1
    std::cout << "*** Test #4 ***" << std::endl;
    count = 0;
    {
        vector v; //<- Heap block not referenced from the stack
        v.elements.push_back(make_block<vector>());
        v.elements.push_back(make_block<vector>());
        v.elements.push_back(make_block<vector>());
        v.elements.push_back(v.elements.back());
    }
    std::cout << count << std::endl;
#endif
#if 1
    std::cout << "*** Test #5 ***" << std::endl;
    count = 0;
    {
        node * v = new node; //<- Heap block not referenced from the stack
        v->next = make_block<node>();
        v->next->next = v->next;
        v->next->prior = v->next;
        v->prior = make_block<node>();
        v->prior->next = v->next;
        v->prior->prior = v->next;
        v->prior.reset();
        std::cout << "node = " << v->next.get() << std::endl;
        std::cout << "node = " << v->next->next.get() << std::endl;
        delete v;
    }
    std::cout << count << std::endl;
#endif
#if 1
    std::cout << "*** Test #6 ***" << std::endl;
    count = 0;
    {
        block_ptr<int> test = make_block<int>(5);
        test = test;
        
        std::cout << "test = " << * test << std::endl;
    }
    std::cout << count << std::endl;
#endif
#if 1
    std::cout << "*** Test #7 ***" << std::endl;
    count = 0;
    for(int i = 0; i < 500; ++i) {
        boost::mpl::for_each<boost::mpl::range_c<int, 1, 100> >(create_type());
    }
    std::cout << count << std::endl;
#endif
    //_exit(-1); // bypassing bug in pool destructor
}
