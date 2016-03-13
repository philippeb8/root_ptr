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
    node(block_proxy const & x) : prior(x), next(x) {
        ++count;
    }
    ~node() {
        --count;
    }
    block_ptr<node> prior;
    block_ptr<node> next;
};

struct list : block_proxy {
public:
    list() : root(*this) {}
    void clear() {
        root.reset();
    }
    void insert() {
        if(root.get() == 0) {
            root = new block<node>(*this);
        } else {
            root->next = new block<node>(*this);
            root->next->prior = root;
            root = root->next;
        }
    }
    ~list()
    {
    }
private:
    block_ptr<node> root;
};

struct vector {
    vector() { ++count; std::cout << __FUNCTION__ << "(): " << this << std::endl; }
    ~vector() { --count; std::cout << __FUNCTION__ << "(): " << this << std::endl; }
    vector(const vector& other) : elements(other.elements) { ++count; }
    boost::container::list<block_ptr<vector> > elements;
};

struct create_type {
    create_type(block_proxy & x) : x_(x) {}
    
    template<class T>
    void operator()(T) const {
        block_ptr<boost::array<char, T::value> >(x_, new block<boost::array<char, T::value> >());
    }
    
    block_proxy & x_;
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
        proxy_ptr<node> x = proxy_ptr<node>(new block<node>(x));
        x->next = x;
    }
    std::cout << count << std::endl;
#endif
#if 1
    std::cout << "*** Test #3 ***" << std::endl;
    count = 0;
    {
        proxy_ptr<vector> v = proxy_ptr<vector>(new block<vector>());
        v->elements.push_back(block_ptr<vector>(v, new block<vector>()));
        v->elements.push_back(block_ptr<vector>(v, new block<vector>()));
        v->elements.push_back(v->elements.back());
        v->elements.push_back(v);
    }
    std::cout << count << std::endl;

    count = 0;
    {
        proxy_ptr<vector> v = proxy_ptr<vector>(new block<vector>());
        v->elements.push_back(v);
    }
    std::cout << count << std::endl;

#endif
#if 1
    std::cout << "*** Test #4 ***" << std::endl;
    count = 0;
    {
        proxy_ptr<vector> x;
        vector v;
        v.elements.push_back(block_ptr<vector>(x, new block<vector>()));
        v.elements.push_back(block_ptr<vector>(x, new block<vector>()));
        v.elements.push_back(block_ptr<vector>(x, new block<vector>()));
        v.elements.push_back(v.elements.back());
    }
    std::cout << count << std::endl;
#endif
#if 1
    std::cout << "*** Test #5 ***" << std::endl;
    count = 0;
    {
        proxy_ptr<node> x;
        node * v = new node(x);
        v->next = new block<node>(x);
        v->next->next = v->next;
        v->next->prior = v->next;
        v->prior = new block<node>(x);
        v->prior->next = v->next;
        v->prior->prior = v->next;
        v->prior.reset();
        std::cout << "node = " << v->prior.get() << std::endl;
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
        proxy_ptr<int> test = proxy_ptr<int>(new block<int>(5));
        test = test;
        
        std::cout << "test = " << * test << std::endl;
    }
    std::cout << count << std::endl;
#endif
#if 1
    std::cout << "*** Test #7 ***" << std::endl;
    count = 0;
    {
        proxy_ptr<int> x;
        for(int i = 0; i < 500; ++i) {
            boost::mpl::for_each<boost::mpl::range_c<int, 1, 100> >(create_type(x));
        }
    }
    std::cout << count << std::endl;
#endif
    //_exit(-1); // bypassing bug in pool destructor
}
