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
    Allocator(int& state_)
        : state(state_) { }
    template<class U>
    Allocator(const Allocator<U>& other)
        : state(other.state) { }
    T* allocate(std::size_t n) {
        if (auto p = std::malloc(sizeof(T) * n)) {
            ++state;
            return static_cast<T*>(p);
        } else {
            throw std::bad_alloc();
        }
    }
    void deallocate(T* p, std::size_t) {
        std::free(p);
        --state;
    }
private:
    int& state;
};

template <typename T>
    class nodealloc : public boost::node<T, Allocator<boost::node<T> > >
    {
        typedef boost::node<T, Allocator<boost::node<T> > > base;
        
    public:
        using typename boost::node<T, Allocator<boost::node<T> > >::PoolType;
        
        PoolType & a_;
        
        nodealloc(PoolType & a) : base(), a_(a)
        {
        }
        
        void * operator new (size_t s, PoolType & a)
        {
            return a.allocate(s);
        }
        
        void operator delete (void * p)
        {
            static_cast<nodealloc *>(p)->a_.deallocate(static_cast<nodealloc *>(p), sizeof(nodealloc));
        }
    };

int main()
{
    int n1 = 0;
    int n2 = 0;
    {
        boost::root_ptr<int> p1, p2, p3;
        
        nodealloc<int>::PoolType a1(n1);
        p1 = new (a1) nodealloc<int>(a1);

        nodealloc<int>::PoolType a2(n2);
        p2 = new (a2) nodealloc<int>(a2);

        nodealloc<int>::PoolType a3(n2);
        p3 = new (a3) nodealloc<int>(a3);

        if (n1 != 1 || n2 != 2) {
            throw 3;
        }
    }
    if (n1 != 0 || n2 != 0) {
        throw 4;
    }
}
