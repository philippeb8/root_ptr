/**
    @file
    benchmark.cpp

    @note
    Copyright (c) 2011-2016 Phil Bouchard <pbouchard8@gmail.com>.

    Distributed under the Boost Software License, Version 1.0.

    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt

    See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.


    Thanks to: Glen Fernandes <glen.fernandes@gmail.com>
*/

#include <chrono>
#include <iostream>
#include <memory>
#include <boost/shared_ptr.hpp>
#include <boost/smart_ptr/root_ptr.hpp>
#include <boost/make_shared.hpp>

template<class T = std::chrono::high_resolution_clock>
class timer {
public:
    timer()
       : start(T::now()) { }
    typename T::duration operator()() const {
        return T::now() - start;
    }
    void reset() {
        start = T::now();
    }
private:
    typename T::time_point start;
};

template<class T>
double benchmark()
{
    constexpr unsigned int count = 5000000;
    T task;
    double total = 0;
    for (unsigned int i = 0; i < count; i++) {
        timer<> watch;
        task();
        total += watch().count();
    }
    return total / count;
}

namespace std
{
template <typename T>
    std::unique_ptr<T> make_unique()
    {
        return std::unique_ptr<T>(new T);
    }
}

template<class T>
struct unique_new {
    void operator()() {
        p.reset(new T());
    }
    std::unique_ptr<T> p;
};

template<class T>
struct unique_make {
    void operator()() {
        p = std::make_unique<T>();
    }
    std::unique_ptr<T> p;
};

template<class T>
struct shared_new {
    void operator()() {
        p.reset(new T());
    }
    boost::shared_ptr<T> p;
};

template<class T>
struct shared_make {
    void operator()() {
        p = boost::make_shared<T>();
    }
    boost::shared_ptr<T> p;
};

template<class T>
struct shared_make_alloc_noinit {
    void operator()() {
        p = boost::allocate_shared_noinit<T>(a);
    }
    boost::shared_ptr<T> p;
    boost::fast_pool_allocator<T> a;
};

template<class T>
struct root_new {
    void operator()() {
        p.reset(new boost::fastnode<T>());
    }
    boost::root_ptr<T> p;
};

template<class T>
struct root_make {
    void operator()() {
        p = boost::make_node<T>();
    }
    boost::root_ptr<T> p;
};

int main()
{
    std::cout << "unique_ptr (new): "
        << benchmark<unique_new<int> >()
        << "\nunique_ptr (make_unique): "
        << benchmark<unique_make<int> >()
        << "\nshared_ptr (new): "
        << benchmark<shared_new<int> >()
        << "\nshared_ptr (make_shared): "
        << benchmark<shared_make<int> >()
        << "\nshared_ptr (allocate_shared_noinit): "
        << benchmark<shared_make_alloc_noinit<int> >()
        << "\nroot_ptr (new): "
        << benchmark<root_new<int> >()
        //<< "\nroot_ptr (make_node): "
        //<< benchmark<root_make<int> >()
        << std::endl;
}
