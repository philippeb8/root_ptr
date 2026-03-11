/**
    \file
    \brief Boost detail/node_base.hpp header file.
    
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


namespace boost
{

    
struct node_proxy;


/**
    Root class of all pointee objects.
*/

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

    virtual void proxy(node_proxy const &) = 0;

    virtual ~node_base()
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
    Pointee object wrapper.
*/

template <typename T>
    class node_element : public node_base
    {
    public:
        typedef T data_type;

        
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
        /** Pointee object.*/
        data_type elem_;
    };

    
template <typename T, size_t S>
    class node_element<std::array<T, S>> : public node_base
    {
    public:
        typedef std::array<T, S> data_type;


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
        /** Pointee object.*/
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
        /** Pointee object.*/
        data_type elem_;
    };


/**
    Pointee object & allocator wrapper.
    
    Main class used to instanciate pointee objects and a copy of the allocator desired.
*/

template <typename T, typename PoolAllocator = pool_allocator<T> >
    class node : public node_element<T>
    {
        typedef node_element<T> base;
        
    public:
        typedef T data_type;
        typedef typename PoolAllocator::template rebind< node<T, PoolAllocator> >::other allocator_type;

        
        virtual void * element()
        {
            return reinterpret_cast<void *>(& this->base::elem_);
        }

        virtual void proxy(node_proxy const & p)
        {
            boost::proxy<data_type>()(p, * static_cast<data_type *>(element()));
        }
        
        /**
            Initialization of a pointee object.
            
            @note Will use a static copy of the allocator which has no parameter.
        */
        
        node() 
        : a_(static_pool())
        {
        }
        

        /**
            Initialization of a pointee object.
            
            @param  a   Allocator to copy.
        */
        
        node(allocator_type const & a) 
        : a_(a)
        {
        }


        template <typename... Args>
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

        
        /**
            Destructor.
        */
        
        virtual ~node()
        {
        }


        /**
            Allocates a new @c node using the static copy of @c PoolAllocator to be used.
            
            @param  s   Disregarded.
            @return     Pointer of the new @c node.
        */

        void * operator new (size_t s)
        {
            void * p = static_pool().allocate(1);

            return p;
        }


        /**
            Allocates a new @c node .
            
            @param  s   Disregarded.
            @param  a   Copy of @c PoolAllocator to be used.
            @return     Pointer of the new @c node.
        */

        void * operator new (size_t s, allocator_type a)
        {
            void * p = a.allocate(1);

            return p;
        }


        /**
            Deallocates a @c node from @c PoolAllocator .
            
            @param  p   Address of the @c node to deallocate.
        */
        
        void operator delete (void * p)
        {
	    static_pool().deallocate(static_cast<node *>(p), 1);
        }


        /**
            Deallocates a @c node from @c PoolAllocator .

            @param  p   Address of the @c node to deallocate.
            @param  a   Copy of @c PoolAllocator to be used.
        */

        void operator delete (void * p, allocator_type a)
        {
            a.deallocate(static_cast<node *>(p), 1);
        }

        
    private:
        /** 
            Static pool.
            
            This is where all @c node are allocated when @c PoolAllocator is not 
            explicitly specified in the constructor. 
        */
         
        static allocator_type & static_pool()
        {
            static allocator_type pool_;
            
            return pool_;
        }


        /** Copy of the @c PoolAllocator to be used. */
        allocator_type a_;        
    };


template <typename T, size_t S, typename PoolAllocator>
    class node<std::array<T, S>, PoolAllocator> : public node_element<std::array<T, S>>
    {
        typedef node_element<std::array<T, S>> base;

    public:
        typedef std::array<T, S> data_type;
        typedef typename PoolAllocator::template rebind< node<std::array<T, S>, PoolAllocator> >::other allocator_type;


        virtual void * element()
        {
            return reinterpret_cast<void *>(& this->base::elem_);
        }

        virtual void proxy(node_proxy const & p)
        {
            boost::proxy<data_type>()(p, * static_cast<data_type *>(element()));
        }

        /**
            Initialization of a pointee object.

            @note Will use a static copy of the allocator which has no parameter.
        */

        node()
            : a_(static_pool())
        {
        }


        /**
            Initialization of a pointee object.

            @param  a   Allocator to copy.
        */

        node(allocator_type const & a)
            : a_(a)
            {
            }


        template <typename... Args>
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


        /**
            Destructor.
        */

        virtual ~node()
        {
        }


        /**
            Allocates a new @c node using the static copy of @c PoolAllocator to be used.

            @param  s   Disregarded.
            @return     Pointer of the new @c node.
        */

        void * operator new (size_t s)
        {
            void * p = static_pool().allocate(1);

            return p;
        }


        /**
            Allocates a new @c node .

            @param  s   Disregarded.
            @param  a   Copy of @c PoolAllocator to be used.
            @return     Pointer of the new @c node.
        */

        void * operator new (size_t s, allocator_type a)
        {
            void * p = a.allocate(1);

            return p;
        }


        /**
            Deallocates a @c node from @c PoolAllocator .

            @param  p   Address of the @c node to deallocate.
        */

        void operator delete (void * p)
        {
            static_pool().deallocate(static_cast<node *>(p), 1);
        }


        /**
            Deallocates a @c node from @c PoolAllocator .

            @param  p   Address of the @c node to deallocate.
            @param  a   Copy of @c PoolAllocator to be used.
        */

        void operator delete (void * p, allocator_type a)
        {
            a.deallocate(static_cast<node *>(p), 1);
        }


    private:
        /**
            Static pool.

            This is where all @c node are allocated when @c PoolAllocator is not
            explicitly specified in the constructor.
        */

        static allocator_type & static_pool()
        {
            static allocator_type pool_;

            return pool_;
        }


        /** Copy of the @c PoolAllocator to be used. */
        allocator_type a_;
    };


} // namespace boost


#endif  // #ifndef BOOST_DETAIL_NODE_BASE_HPP_INCLUDED
