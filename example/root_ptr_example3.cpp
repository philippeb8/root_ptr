/*!
  \file
  \brief Example 3 of root_ptr
*/

/*
  Copyright Phil Bouchard 2016

  Distributed under the Boost Software License, Version 1.0.
  See accompanying file LICENSE_1_0.txt or copy at
  http://www.boost.org/LICENSE_1_0.txt
*/

#include <boost/smart_ptr/root_ptr.hpp>
#include <boost/mpl/range_c.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/array.hpp>
#include <boost/container/vector.hpp>

#include <vector>
#include <iostream>

//#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN
#define BOOST_LIB_DIAGNOSTIC "on" // Report library file details.

#include <boost/test/unit_test.hpp>

static int count;

using namespace boost;

struct list_node {
    list_node(node_proxy const & x) : prior(x), next(x) {
        ++count;
    }
    ~list_node() {
        --count;
    }
    node_ptr<list_node> prior;
    node_ptr<list_node> next;
};

struct list {
public:
    list() {}
    void clear() {
        root.reset();
    }
    void insert() {
        if(root.get() == 0) {
            root = new node<list_node>(root);
        } else {
            root->next = new node<list_node>(root);
            root->next->prior = root;
            root = root->next;
        }
    }
    ~list()
    {
    }
private:
    root_ptr<list_node> root;
};


struct vector {
    vector() { ++count; }
    ~vector() { --count; }
    vector(const vector& other) : elements(other.elements) { ++count; }
    boost::container::vector<node_ptr<vector> > elements;
};

struct create_type {
    create_type(node_proxy & x) : x_(x) {}
    
    template<class T>
    void operator()(T) const {
        node_ptr<boost::array<char, T::value> >(x_, new node<boost::array<char, T::value> >());
    }
    
    node_proxy & x_;
};


BOOST_AUTO_TEST_CASE(test_node_ptr) {

    count = 0;
    {
        root_ptr<vector> v(new node<vector>());
        v->elements.push_back(v);
    }
    BOOST_CHECK_EQUAL(count, 0);

    count = 0;
    {
        list l;
        for(int j = 0; j < 2; ++j) {
            for(int i = 0; i < 2; ++i) {
                l.insert();
            }
            l.clear();
        }
    }
    BOOST_CHECK_EQUAL(count, 0);

    count = 0;
    {
        root_ptr<int> test(new node<int>(5));
        test = test;
        
        BOOST_CHECK_NE(test.get(), static_cast<int*>(0));
        BOOST_CHECK_EQUAL(*test, 5);
    }

    count = 0;
    {
        root_ptr<int> x;
        for(int i = 0; i < 500; ++i) {
            boost::mpl::for_each<boost::mpl::range_c<int, 1, 100> >(create_type(x));
        }
    }
    BOOST_CHECK_EQUAL(count, 0);

    count = 0;
    {
        root_ptr<vector> v(new node<vector>());
        v->elements.push_back(v);
    }
    BOOST_CHECK_EQUAL(count, 0);

    {
        root_ptr<vector> x;
        vector v;
        v.elements.push_back(node_ptr<vector>(x, new node<vector>()));
    }
    BOOST_CHECK_EQUAL(count, 0);
} // BOOST_AUTO_TEST_CASE(test_node_ptr)

