/**
    \file
    Boost intrusive_list.hpp header file.

    Patent US11288049B2
    'SOURCE TO SOURCE COMPILER, COMPILATION METHOD, AND
    COMPUTER-READABLE MEDIUM FOR PREDICTABLE MEMORY MANAGEMENT'
    
    Copyright (C) 2020-2026 Fornux LLC

    Phil Bouchard, Founder & CEO
    Fornux LLC
    phil@fornux.com
    3909 S Maryland Pkwy Ste 314 #638, Las Vegas, NV, 89119
    
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/


#ifndef BOOST_INTRUSIVE_LIST_HPP_INCLUDED
#define BOOST_INTRUSIVE_LIST_HPP_INCLUDED


#ifndef BOOST_DISABLE_THREADS
#include <mutex>
#include <boost/thread/recursive_mutex.hpp>
#endif


#include "classof.hpp"


namespace boost
{


#ifndef BOOST_DISABLE_THREADS
static std::recursive_mutex & static_recursive_mutex();
#endif


namespace smart_ptr
{

namespace detail
{


struct intrusive_list_node
{
    intrusive_list_node * next;
    intrusive_list_node * prev;
    
    intrusive_list_node() : next(this), prev(this)
    {
    }

    intrusive_list_node(intrusive_list_node * const p) : intrusive_list_node()
    {
        insert(p);
    }

    void insert(intrusive_list_node * const p)
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        p->next = this;
        p->prev = prev;
        
        prev->next = p;
        prev = p;
    }

    void erase()
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        prev->next = next;
        next->prev = prev;

        clear();
    }
    
    void clear()
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        next = this;
        prev = this;
    }

    bool singleton() const
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        return next == this && prev == this;
    }
    
    ~intrusive_list_node()
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        erase();
    }
};


/**
    Static list.
    
    Rewritten list template with explicit access to internal nodes.  This 
    allows usages of tags already part of an object, used to group objects 
    together without the need of any memory allocation.
*/

struct intrusive_list : intrusive_list_node
{
    typedef intrusive_list_node base;

    typedef intrusive_list node;
    typedef intrusive_list * pointer;
    template <typename T, intrusive_list T::* P> 
        struct iterator;
    template <typename T, intrusive_list T::* P>
        struct reverse_iterator;
        
    using base::base;


    intrusive_list(intrusive_list const &) = delete;
    
    pointer begin() 
    { 
        return static_cast<pointer>(next); 
        
    }
    
    pointer end()
    {
        return this;
    }

    pointer rbegin()
    {
        return static_cast<pointer>(prev);

    }

    pointer rend()
    {
        return this;
    }

    bool empty() const
    { 
        return singleton();
    }
    
    void push_front(pointer i)
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        i->erase();
        begin()->insert(i);
    }
    
    void push_back(pointer i)
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        i->erase();
        end()->insert(i);
    }
    
    void merge(intrusive_list& x)
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        if (! x.empty())
        {
            x.prev->next = next;
            next->prev = x.prev;
            
            next = x.next;
            x.next->prev = this;
        }
    }

    void splice(intrusive_list& x)
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        if (! x.empty())
        {
            x.prev->next = next;
            next->prev = x.prev;
            
            next = x.next;
            x.next->prev = this;
            
            x.clear();
        }
    }
};


template <typename T, intrusive_list T::* P>
    struct intrusive_list::iterator
    {
        typedef iterator self_type;
        typedef intrusive_list node_type;

        iterator(intrusive_list::pointer __x) 
        : node_(__x) 
        {
        }

        T & operator * () const
        { 
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            return * classof(P, node_); 
        }

        T * operator -> () const
        { 
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            return classof(P, node_); 
        }

        self_type & operator = (self_type const & x)
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            node_ = x.node_;
            
            return * this;
        }

        self_type & operator ++ ()
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            node_ = static_cast<intrusive_list::pointer>(node_->next);
            
            return * this;
        }

        self_type & operator -- ()
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            node_ = static_cast<intrusive_list::pointer>(node_->prev);
            
            return * this;
        }

        bool operator == (const self_type & x) const 
        { 
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            return node_ == x.node_; 
        }
        
        bool operator != (const self_type & x) const 
        { 
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            return node_ != x.node_; 
        }

        node_type * node_;
    };


template <typename T, intrusive_list T::* P>
    struct intrusive_list::reverse_iterator
    {
        typedef reverse_iterator self_type;
        typedef intrusive_list node_type;

        reverse_iterator(intrusive_list::pointer __x)
          : node_(__x)
        {
        }

        T & operator * () const
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

          return * classof(P, node_);
        }

        T * operator -> () const
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            return classof(P, node_);
        }

        self_type & operator = (self_type const & x)
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            node_ = x.node_;
        
            return * this;
        }

        self_type & operator ++ ()
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            node_ = static_cast<intrusive_list::pointer>(node_->prev);
        
            return * this;
        }

        self_type & operator -- ()
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            node_ = static_cast<intrusive_list::pointer>(node_->next);
        
            return * this;
        }

        bool operator == (const self_type & x) const
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

          return node_ == x.node_;
        }

        bool operator != (const self_type & x) const
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            return node_ != x.node_;
        }

        node_type * node_;
    };


} // namespace detail

} // namespace smart_ptr

} // namespace boost


#endif // #ifndef BOOST_INTRUSIVE_LIST_HPP_INCLUDED
