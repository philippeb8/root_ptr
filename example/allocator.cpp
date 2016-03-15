/**
    @file
    allocator.cpp

    @note
    Copyright (c) 2011-2016 Phil Bouchard <pbouchard8@gmail.com>.

    Distributed under the Boost Software License, Version 1.0.

    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt

    See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.


    Thanks to: Glen Fernandes <glen.fernandes@gmail.com>
*/

#include <memory>
#include <boost/smart_ptr/root_ptr.hpp>

void* operator new(std::size_t)
{
    throw 1;
}

void operator delete(void*, std::size_t)
{
    throw 2;
}

template<class T>
class Allocator {
    template<class U>
    friend class Allocator;
public:
    template <class Type> 
        struct rebind 
        {
            typedef Allocator<Type> other;
        };
        
    using value_type = T;
    Allocator(int& state1_, int& state2_)
        : state1(state1_),
          state2(state2_) { }
    template<class U>
    Allocator(const Allocator<U>& other)
        : state1(other.state1),
          state2(other.state2) { }
    T* allocate(std::size_t n) {
        if (auto p = std::malloc(sizeof(T) * n)) {
            ++state1;
            return static_cast<T*>(p);
        } else {
            throw std::bad_alloc();
        }
    }
    void deallocate(T* p, std::size_t) {
        std::free(p);
        --state1;
    }
    template<class U, class... Args>
    void construct(U* p, Args&&... args) {
        ::new(p) U(std::forward<Args>(args)...);
        ++state2;
    }
    template<class U>
    void destroy(U* p) {
        p->~U();
        --state2;
    }
private:
    int& state1;
    int& state2;
};

struct U {
    U(int, char) { }
};

int main()
{
    int n1 = 0, m1 = 0;
    int n2 = 0, m2 = 0;
    {
        boost::root_ptr<U> p1, p2, p3;

        typedef boost::node<U, Allocator<U> > node;
        typedef typename node::allocator_type allocator_type;
        
        allocator_type a1(n1, m1);
        p1 = new (a1) node(a1, 1, 'a');

        allocator_type a2(n2, m2);
        p2 = new (a2) node(a2, 2, 'b');

        allocator_type a3(n2, m2);
        p3 = new (a3) node(a3, 3, 'c');
        
        if (n1 != 1 || m1 != 1 || n2 != 2 || m2 != 2) {
            throw 3;
        }
    }
    if (n1 != 0 || m1 != 0 || n2 != 0 || m2 != 0) {
        throw 4;
    }
}
