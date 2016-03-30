/*!
  \file
  \brief Example 2 of using root_ptr.
*/
/*
  Copyright Phil Bouchard 2016
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
    vector() { ++count; std::cout << __FUNCTION__ << "(): " << this << std::endl; }
    ~vector() { --count; std::cout << __FUNCTION__ << "(): " << this << std::endl; }
    vector(const vector& other) : elements(other.elements) { ++count; }
    boost::container::list<node_ptr<vector> > elements;
};

struct create_type {
    create_type(node_proxy & x) : x_(x) {}
    
    template<class T>
    void operator()(T) const {
        node_ptr<boost::array<char, T::value> >(x_, new node<boost::array<char, T::value> >());
    }
    
    node_proxy & x_;
};

int main()
{
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
        root_ptr<list_node> x;
        node_ptr<list_node> v = node_ptr<list_node>(x, new node<list_node>(x));
        v->next = v;
    }
    std::cout << count << std::endl;
#endif
#if 1
    std::cout << "*** Test #3 ***" << std::endl;
    count = 0;
    {
        root_ptr<vector> v(new node<vector>());
        v->elements.push_back(node_ptr<vector>(v, new node<vector>()));
        v->elements.push_back(node_ptr<vector>(v, new node<vector>()));
        v->elements.push_back(v->elements.back());
        v->elements.push_back(v);
    }
    std::cout << count << std::endl;

    count = 0;
    {
        root_ptr<vector> v(new node<vector>());
        v->elements.push_back(v);
    }
    std::cout << count << std::endl;

#endif
#if 1
    std::cout << "*** Test #4 ***" << std::endl;
    count = 0;
    {
        root_ptr<vector> x;
        vector v;
        v.elements.push_back(node_ptr<vector>(x, new node<vector>()));
        v.elements.push_back(node_ptr<vector>(x, new node<vector>()));
        v.elements.push_back(node_ptr<vector>(x, new node<vector>()));
        v.elements.push_back(v.elements.back());
    }
    std::cout << count << std::endl;
#endif
#if 1
    std::cout << "*** Test #5 ***" << std::endl;
    count = 0;
    {
        root_ptr<list_node> x;
        list_node * v = new list_node(x);
        v->next = new node<list_node>(x);
        v->next->next = v->next;
        v->next->prior = v->next;
        v->prior = new node<list_node>(x);
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
        root_ptr<int> test(new node<int>(5));
        test = test;
        
        std::cout << "test = " << * test << std::endl;
    }
    std::cout << count << std::endl;
#endif
#if 1
    std::cout << "*** Test #7 ***" << std::endl;
    count = 0;
    {
        root_ptr<int> x;
        for(int i = 0; i < 500; ++i) {
            boost::mpl::for_each<boost::mpl::range_c<int, 1, 100> >(create_type(x));
        }
    }
    std::cout << count << std::endl;
#endif
    //_exit(-1); // bypassing bug in pool destructor
} // int main()

