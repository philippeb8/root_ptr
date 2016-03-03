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


#include <boost/block_ptr.hpp>
#include <boost/block_allocator.hpp>

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
	//std::list<block_ptr<vector>, block_allocator< block_ptr<vector> > > elements; //! works fine
	boost::container::list<block_ptr<vector>, block_allocator< block_ptr<vector> > > elements; //! works fine
};

struct create_type {
    template<class T>
    void operator()(T) const {
        make_block<boost::array<char, T::value> >();
    }
};

int main() {
#if 1
    count = 0;
    {
        node n;
        block_ptr<node> v = &n;
        v = v;
    }
    std::cout << count << std::endl;
#endif
#if 1
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
	count = 0;
	{
		block_ptr<node> v = new block<node>();
		v->next = v;
	}
	std::cout << count << std::endl;
#endif
#if 1
	count = 0;
    {
        block_ptr<vector> v = new block<vector>();
        v->elements.push_back(v);
    }
    std::cout << count << std::endl;

	count = 0;
    {
        block_ptr<vector> v = new block<vector>();
        v->elements.push_back(v);
    }
    std::cout << count << std::endl;

#endif
#if 0
	count = 0;
	{
        vector v;
        v.elements.push_back(new block<vector>()); //<- Heap block not referenced from the stack
    }
    std::cout << count << std::endl;
#endif
#if 1
    count = 0;
    {
        block_ptr<int> test = make_block<int>(5);
        test = test;
        
        std::cout << "test = " << * test << std::endl;
    }
    std::cout << count << std::endl;
#endif
#if 1
    count = 0;
    for(int i = 0; i < 500; ++i) {
        boost::mpl::for_each<boost::mpl::range_c<int, 1, 100> >(create_type());
    }
    std::cout << count << std::endl;
#endif
    //_exit(-1); // bypassing bug in pool destructor
}
