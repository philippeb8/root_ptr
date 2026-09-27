/**
    @file
    @brief Nodes that own the objects managed by @c boost::root_ptr.

    Patent US11288049B2
    'SOURCE TO SOURCE COMPILER, COMPILATION METHOD, AND
    COMPUTER-READABLE MEDIUM FOR PREDICTABLE MEMORY MANAGEMENT'

    @copyright Copyright (C) 2020-2026 Services Informatiques Fornux

    @author Phil Bouchard, Founder & CEO
    Services Informatiques Fornux
    phil@fornux.com
    20 Poirier St., Gatineau, Quebec, Canada, J8V 1A6

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


#ifndef BOOST_DETAIL_NODE_BASE_HPP_INCLUDED
#define BOOST_DETAIL_NODE_BASE_HPP_INCLUDED

// MS compatible compilers support #pragma once

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <limits>
#include <utility>
#include <iterator>
#include <type_traits>

#ifndef BOOST_DISABLE_THREADS
#include <boost/thread.hpp>
#include <boost/thread/tss.hpp>
#else
#include <memory>
#endif

#include <exception>
#include <cstring>
#include <boost/pool/pool.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/numeric/interval.hpp>
#include <boost/type_traits/is_array.hpp>
#include <boost/type_traits/remove_extent.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>
#include <boost/smart_ptr/detail/sp_counted_base.hpp>
#include <boost/preprocessor/control/expr_if.hpp>
#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>
#include <boost/concept_check.hpp>
#include <boost/container/allocator_traits.hpp>
#include <boost/tti/has_static_member_function.hpp>

#include <boost/smart_ptr/detail/intrusive_list.hpp>
#include <boost/smart_ptr/page_allocator.hpp>

/**
    @brief Allocator template used by @c node when none is given.

    Defaults to @c boost::page_allocator_by_size: it frees in constant time,
    where @c boost::pool_allocator's ordered free makes releasing many nodes
    quadratic, and it uses less memory. Define it on the command line to run a
    whole program on another one, e.g.
    @c -DBOOST_ROOT_PTR_ALLOCATOR=boost::pool_allocator.
*/
#ifndef BOOST_ROOT_PTR_ALLOCATOR
#define BOOST_ROOT_PTR_ALLOCATOR boost::page_allocator_by_size
#endif


namespace boost
{

    
struct node_proxy;

// Forward declarations. Collection is driven by the root sets of node_proxy
// and by reference counts; no per-class member visitor is needed.
template <typename T> class root_ptr;
template <typename T, size_t S> class root_array;


namespace smart_ptr
{
namespace detail
{

/** @brief Whether the first type of @c Args is @c A, ignoring references and cv-qualifiers. */
template <typename A, typename... Args>
    struct leads_with : std::false_type {};

template <typename A, typename First, typename... Rest>
    struct leads_with<A, First, Rest...> : std::is_same<A, typename std::decay<First>::type> {};

/**
    @brief Exception thrown by a managed destructor during collection.

    node::destroy() is noexcept, so it stores the exception here and
    ~node_proxy() rethrows it where user code can catch it.
*/
inline std::exception_ptr & pending_destructor_exception()
{
    static thread_local std::exception_ptr e;

    return e;
}

} // namespace detail
} // namespace smart_ptr


/** @brief Root class of all pointee objects. */

struct node_base : public boost::detail::sp_counted_base
{
#ifdef BOOST_REPORT
    bool explicit_delete_ = false;
#endif

    node_base()
    {
    }

    virtual size_t size() const = 0;

    virtual size_t size_bytes() const = 0;
    
    virtual void const * data() = 0;
    
    virtual void * element() = 0;

    /** @brief Destructor; @c noexcept(false) so that a throwing managed destructor reaches node::destroy(). */
    virtual ~node_base() noexcept(false)
    {
    }

    virtual void dispose() BOOST_SP_NOEXCEPT
    {
    }

    virtual void destroy() BOOST_SP_NOEXCEPT
    {
        delete this;
    }
    
protected:
    virtual void * get_deleter(std::type_info const &) BOOST_SP_NOEXCEPT
    { 
        return 0; 
    }
    
    virtual void * get_local_deleter(std::type_info const &) BOOST_SP_NOEXCEPT
    { 
        return 0; 
    }
    
    virtual void * get_untyped_deleter() BOOST_SP_NOEXCEPT
    { 
        return 0; 
    }
};


#define TEMPLATEARGUMENT_DECL(z, n, text) BOOST_PP_COMMA_IF(n) T ## n
#define TEMPLATE_DECL(z, n, text) BOOST_PP_COMMA_IF(n) typename T ## n
#define ARGUMENT_DECL(z, n, text) BOOST_PP_COMMA_IF(n) T ## n const & t ## n
#define PARAMETER_DECL(z, n, text) BOOST_PP_COMMA_IF(n) t ## n

#define CONSTRUCT_NODE3(z, n, text)                                                                                             \
    template <BOOST_PP_REPEAT(n, TEMPLATE_DECL, 0)>                                                                             \
        text(BOOST_PP_REPEAT(n, ARGUMENT_DECL, 0)) : base(BOOST_PP_REPEAT(n, PARAMETER_DECL, 0)) {}                                                                                                        

#define CONSTRUCT_NODE4(z, n, text)                                                                                             \
    template <BOOST_PP_REPEAT(n, TEMPLATE_DECL, 0)>                                                                             \
        text(allocator_type const & a, BOOST_PP_REPEAT(n, ARGUMENT_DECL, 0)) : base(a, BOOST_PP_REPEAT(n, PARAMETER_DECL, 0))   \
        {                                                                                                                       \
        }                                                                                                        

#define ALLOCATE_NODE1(z, n, text)                                                                                              \
    template <BOOST_PP_REPEAT(n, TEMPLATE_DECL, 0)>                                                                             \
        static node * text(allocator_type const & a, BOOST_PP_REPEAT(n, ARGUMENT_DECL, 0))                                      \
        {                                                                                                                       \
            return new (a) node(a, BOOST_PP_REPEAT(n, PARAMETER_DECL, 0));                                                      \
        }

#define MAKE_NODE_ALLOCATOR1(z, n, text)                                                                                        \
    template<template <typename, typename...> class Alloc, typename T, BOOST_PP_REPEAT(n, TEMPLATE_DECL, 0), typename... Args>  \
        typename node<T, Alloc<T, BOOST_PP_REPEAT(n, TEMPLATEARGUMENT_DECL, 0)> >::allocator_type text(Args&&... args)          \
        {                                                                                                                       \
            return typename node<T, Alloc<T, BOOST_PP_REPEAT(n, TEMPLATEARGUMENT_DECL, 0)> >::allocator_type(args...);          \
        }
    

/**
    @brief Tag selecting in-place construction of the element from a callable.

    The callable returns a prvalue that initializes the element directly
    (guaranteed copy elision), so the element needs neither a copy nor a
    move constructor.
*/

struct emplace_tag {};

/** @brief Pointee object wrapper. */

template <typename T>
    class node_element : public node_base
    {
    public:
        typedef T data_type;

        
        template <typename F>
            node_element(emplace_tag, F && f)
            : elem_{f()}
            {
            }

        template <typename... Args>
            node_element(Args &&... args)
            : elem_{std::forward<Args>(args)...}
            {
            }
            
        virtual size_t size() const
        {
            return 1;
        }
        
        virtual size_t size_bytes() const
        {
            return sizeof(T);
        }
        
        virtual void const * data()
        {
            return & elem_;
        }
        

    protected:
        /// Pointee object.
        data_type elem_;
    };

    
template <typename T, size_t S>
    class node_element<std::array<T, S>> : public node_base
    {
    public:
        typedef std::array<T, S> data_type;


        /**
            @brief Builds the whole @c std::array in place from the prvalue returned by @p f.

            Used for every array initializer: forwarding one argument per element
            makes the compiler fail on very large tables.
        */

        template <typename F>
            node_element(emplace_tag, F && f)
            : elem_{f()}
            {
            }

        template <typename... Args>
            node_element(Args &&... args)
            : elem_{std::forward<Args>(args)...}
            {
            }
            
        virtual size_t size() const
        {
            return S;
        }
        
        virtual size_t size_bytes() const
        {
            return S * sizeof(T);
        }
        
        virtual void const * data()
        {
            return reinterpret_cast<data_type *>(& elem_)->data();
        }
        
        
    protected:
        /// Pointee object.
        data_type elem_;
    };

    
template <typename T>
    class node_element<std::vector<T>> : public node_base
    {        
    public:
        typedef std::vector<T> data_type;
        
        
        template <typename... Args>
            node_element(Args &&... args)
            : elem_{std::forward<Args>(args)...}
            {
            }
            
        virtual size_t size() const
        {
            return elem_.size();
        }
        
        virtual size_t size_bytes() const
        {
            return elem_.size() * sizeof(T);
        }
        
        virtual void const * data()
        {
            return elem_.data();
        }
        
        
    protected:
        /// Pointee object.
        data_type elem_;
    };


/**
    @brief Pointee object and allocator wrapper.

    Main class used to instantiate pointee objects, with a copy of the
    desired allocator.
*/

template <typename T, typename PoolAllocator = BOOST_ROOT_PTR_ALLOCATOR<T> >
    class node : public node_element<T>
    {
        typedef node_element<T> base;
        
    public:
        typedef T data_type;
        /// The allocator, rebound to this node type. allocator_traits works for any
        /// allocator; std::allocator has no rebind member since C++20.
        typedef typename std::allocator_traits<PoolAllocator>::template rebind_alloc< node<T, PoolAllocator> > allocator_type;

        
        virtual void * element()
        {
            return reinterpret_cast<void *>(& this->base::elem_);
        }

        /** @brief Constructs the pointee object using the static allocator. */
        
        node() 
        : a_(static_pool())
        {
        }
        

        /**
            @brief Constructs the pointee object.

            @param a Allocator to copy.
        */
        
        node(allocator_type const & a) 
        : a_(a)
        {
        }


        /// Excluded when the first argument is the allocator: the overload below takes it.
        template <typename... Args,
                  typename std::enable_if<! smart_ptr::detail::leads_with<allocator_type, Args...>::value, int>::type = 0>
            node(Args &&... args)
            : a_(static_pool())
            , node_element<T>{std::forward<Args>(args)...}
            {
            }
            

        template <typename... Args>
            node(allocator_type const & a, Args &&... args)
            : a_(a)
            , node_element<T>{std::forward<Args>(args)...}
            {
            }

        
        /** @brief Destructor. */
        
        virtual ~node()
        {
        }

        /**
            @brief Destroys the pointee object.

            A throwing destructor is caught and stored; ~node_proxy() rethrows it. The
            memory goes back to the allocator the node was built with.
        */
        virtual void destroy() BOOST_SP_NOEXCEPT
        {
            // ~node() destroys a_, so keep a copy to free the memory with.
            allocator_type a(a_);

            try
            {
                this->~node();
            }
            catch (...)
            {
                if (! smart_ptr::detail::pending_destructor_exception())
                    smart_ptr::detail::pending_destructor_exception() = std::current_exception();
            }

#ifdef BOOST_ZEROIZATION
            std::memset(this, 0, sizeof(*this));
#endif
            a.deallocate(this, 1);
        }


        /**
            @brief Allocates a node from the static allocator.

            @param s Ignored.
            @return Address of the new node.
        */

        void * operator new (size_t s)
        {
            void * p = static_pool().allocate(1);

            return p;
        }


        /**
            @brief Allocates a node from @p a.

            @param s Ignored.
            @param a Allocator to use.
            @return Address of the new node.
        */

        void * operator new (size_t s, allocator_type a)
        {
            void * p = a.allocate(1);

            return p;
        }


        /**
            @brief Deallocates a node from the static allocator.

            @param p Address of the node.
        */
        
        void operator delete (void * p)
        {
	    static_pool().deallocate(static_cast<node *>(p), 1);
        }


        /**
            @brief Deallocates a node from @p a.

            @param p Address of the node.
            @param a Allocator to use.
        */

        void operator delete (void * p, allocator_type a)
        {
            a.deallocate(static_cast<node *>(p), 1);
        }

        
    private:
        /** @brief Static allocator, used when none is given to the constructor. */
         
        static allocator_type & static_pool()
        {
            static allocator_type pool_;
            
            return pool_;
        }


        /// Copy of the allocator in use.
        allocator_type a_;        
    };


template <typename T, size_t S, typename PoolAllocator>
    class node<std::array<T, S>, PoolAllocator> : public node_element<std::array<T, S>>
    {
        typedef node_element<std::array<T, S>> base;

    public:
        typedef std::array<T, S> data_type;
        /// The allocator, rebound to this node type.
        typedef typename std::allocator_traits<PoolAllocator>::template rebind_alloc< node<std::array<T, S>, PoolAllocator> > allocator_type;


        virtual void * element()
        {
            return reinterpret_cast<void *>(& this->base::elem_);
        }

        /** @brief Constructs the pointee object using the static allocator. */

        node()
            : a_(static_pool())
        {
        }


        /**
            @brief Constructs the pointee object.

            @param a Allocator to copy.
        */

        node(allocator_type const & a)
            : a_(a)
            {
            }


        /// Excluded when the first argument is the allocator: the overload below takes it.
        template <typename... Args,
                  typename std::enable_if<! smart_ptr::detail::leads_with<allocator_type, Args...>::value, int>::type = 0>
            node(Args &&... args)
                : a_(static_pool())
                , node_element<data_type>{std::forward<Args>(args)...}
            {
            }


        template <typename... Args>
            node(allocator_type const & a, Args &&... args)
                : a_(a)
                , node_element<data_type>{std::forward<Args>(args)...}
            {
            }


        template <size_t... I>
            node(T (& v)[S], std::index_sequence<I...>)
                : base{v[I]...}
                {
                }


            node(T (& v)[S])
                : node(v, std::make_index_sequence<S>{})
                {
                }


        /** @brief Destructor. */

        virtual ~node()
        {
        }

        /**
            @brief Destroys the pointee object.

            A throwing destructor is caught and stored; ~node_proxy() rethrows it. The
            memory goes back to the allocator the node was built with.
        */
        virtual void destroy() BOOST_SP_NOEXCEPT
        {
            // ~node() destroys a_, so keep a copy to free the memory with.
            allocator_type a(a_);

            try
            {
                this->~node();
            }
            catch (...)
            {
                if (! smart_ptr::detail::pending_destructor_exception())
                    smart_ptr::detail::pending_destructor_exception() = std::current_exception();
            }

#ifdef BOOST_ZEROIZATION
            std::memset(this, 0, sizeof(*this));
#endif
            a.deallocate(this, 1);
        }


        /**
            @brief Allocates a node from the static allocator.

            @param s Ignored.
            @return Address of the new node.
        */

        void * operator new (size_t s)
        {
            void * p = static_pool().allocate(1);

            return p;
        }


        /**
            @brief Allocates a node from @p a.

            @param s Ignored.
            @param a Allocator to use.
            @return Address of the new node.
        */

        void * operator new (size_t s, allocator_type a)
        {
            void * p = a.allocate(1);

            return p;
        }


        /**
            @brief Deallocates a node from the static allocator.

            @param p Address of the node.
        */

        void operator delete (void * p)
        {
            static_pool().deallocate(static_cast<node *>(p), 1);
        }


        /**
            @brief Deallocates a node from @p a.

            @param p Address of the node.
            @param a Allocator to use.
        */

        void operator delete (void * p, allocator_type a)
        {
            a.deallocate(static_cast<node *>(p), 1);
        }


    private:
        /** @brief Static allocator, used when none is given to the constructor. */

        static allocator_type & static_pool()
        {
            static allocator_type pool_;

            return pool_;
        }


        /// Copy of the allocator in use.
        allocator_type a_;
    };


} // namespace boost


#endif  // #ifndef BOOST_DETAIL_NODE_BASE_HPP_INCLUDED
