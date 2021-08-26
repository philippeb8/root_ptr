/**
    \file
    \brief Boost detail/node_base.hpp header file.
    
    Patent Pending
    'SOURCE TO SOURCE COMPILER, COMPILATION METHOD, AND
    COMPUTER-READABLE MEDIUM FOR PREDICTABLE MEMORY MANAGEMENT'
    
    Copyright (C) 2021 Fornux Inc.
    
    Phil Bouchard, Founder & CTO
    Fornux Inc.
    phil@fornux.com
    20 Poirier St, Gatineau, Quebec, Canada, J8V 1A6
    
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

#include <array>
#include <limits>
#include <utility>
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
#include <boost/smart_ptr/detail/intrusive_stack.hpp>


namespace boost
{

    
struct node_proxy;

template <typename T>
    class root_ptr;
    
template <typename T, size_t S>
    class root_array;

    
BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION(__proxy)


template <typename T, bool = has_static_member_function___proxy<typename std::remove_reference<T>::type, typename std::remove_reference<T>::type const * (node_proxy &, typename std::remove_reference<T>::type const *)>::value>
    struct proxy
    {
        inline typename std::remove_reference<T>::type const * operator () (node_proxy * x, typename std::remove_reference<T>::type const * po) const
        {
            return po; 
        }
    };

template <>
    struct proxy<void, false>
    {
        inline void const * operator () (node_proxy * x, void const * po)
        {
            return po;
        }
    };

template <typename T, size_t S>
    struct proxy<std::array<T, S>, false>
    {
        inline std::array<T, S> const * operator () (node_proxy * x, std::array<T, S> const * po) const
        {
            for (typename std::array<T, S>::const_iterator i = po->begin(); i != po->end(); ++ i)
                proxy<T>()(x, &* i);
            
            return po;
        }
    };

template <typename T>
    struct proxy<std::vector<T>, false>
    {
        inline std::vector<T> const * operator () (node_proxy * x, std::vector<T> const * po) const
        {
            for (typename std::vector<T>::const_iterator i = po->begin(); i != po->end(); ++ i)
                proxy<T>()(x, &* i);
            
            return po;
        }
    };

template <>
    struct proxy<std::vector<void>, false>
    {
        inline std::vector<void> const * operator () (node_proxy * x, std::vector<void> const * po) const
        {
            return po;
        }
    };
    
template <typename T>
    struct proxy<root_ptr<T>, false>
    {
        inline root_ptr<T> const * operator () (node_proxy * x, root_ptr<T> const * po) const
        {
            po->proxy(x);
            
            return po;
        }
    };

template <typename T, size_t S>
    struct proxy<root_array<T, S>, false>
    {
        inline root_array<T, S> const * operator () (node_proxy * x, root_array<T, S> const * po) const
        {
            po->proxy(x);
            
            return po;
        }
    };

template <typename T>
    struct proxy<T, true>
    {
        inline T const * operator () (node_proxy * x, T const * po) const
        { 
            T::__proxy(* x, po);
            
            return po;
        }
    };

template <typename T>
    inline T & make_proxy(node_proxy * x, T & po)
    { 
        return * const_cast<T *>(proxy<T>()(x, & po));
    }

template <typename T>
    inline T const & make_proxy(node_proxy * x, T const & po)
    {
        return * proxy<T>()(x, const_cast<T *>(& po));
    }

    
/**
    Root class of all pointee objects.
*/

struct node_base : public boost::detail::sp_counted_base
{
    /** Tag used to enlist to @c node_proxy::node_list_ . */
    smart_ptr::detail::intrusive_list::node node_tag_;
    
#ifdef BOOST_REPORT
    bool explicit_delete_ = false;
#endif

    virtual size_t size() const = 0;

    virtual size_t size_bytes() const = 0;
    
    virtual void * data() = 0;
    
    virtual void * element() = 0;
    
    virtual void const * proxy(node_proxy *) = 0;
    
    virtual ~node_base()
    {
    }

protected:
    virtual void dispose()
    {
    }

    virtual void destroy() // nothrow
    {
        delete this;
    }
    
    virtual void * get_deleter(std::type_info const &)
    { 
        return 0; 
    }
    
    virtual void * get_local_deleter(std::type_info const &)
    { 
        return 0; 
    }
    
    virtual void * get_untyped_deleter()
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

        
        virtual size_t size() const
        {
            return 1;
        }
        
        virtual size_t size_bytes() const
        {
            return sizeof(T);
        }
        
        virtual void * data()
        {
            return & elem_;
        }
        

    protected:
        /** Pointee object.*/
        typename std::aligned_storage<sizeof(data_type), alignof(data_type)>::type elem_;
    };

    
template <typename T, size_t S>
    class node_element<std::array<T, S>> : public node_base
    {
    public:
        typedef std::array<T, S> data_type;

        virtual size_t size() const
        {
            return S;
        }
        
        virtual size_t size_bytes() const
        {
            return S * sizeof(T);
        }
        
        virtual void * data()
        {
            return reinterpret_cast<data_type *>(& elem_)->data();
        }
        
        
    protected:
        /** Pointee object.*/
        typename std::aligned_storage<sizeof(data_type), alignof(data_type)>::type elem_;
    };

    
template <typename T>
    class node_element<std::vector<T>> : public node_base
    {        
    public:
        typedef std::vector<T> data_type;
        
        virtual size_t size() const
        {
            return reinterpret_cast<data_type const *>(& elem_)->size();
        }
        
        virtual size_t size_bytes() const
        {
            return reinterpret_cast<data_type const *>(& elem_)->size() * sizeof(T);
        }
        
        virtual void * data()
        {
            return reinterpret_cast<data_type *>(& elem_)->data();
        }
        
        
    protected:
        /** Pointee object.*/
        typename std::aligned_storage<sizeof(data_type), alignof(data_type)>::type elem_;
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
        
        virtual void const * proxy(node_proxy * p)
        {
            return reinterpret_cast<void const *>(boost::proxy<data_type>()(p, static_cast<data_type *>(element())));
        }
        
        
        /**
            Initialization of a pointee object.
            
            @note Will use a static copy of the allocator which has no parameter.
        */
        
        node() 
        : a_(static_pool())
        {
            container::allocator_traits<allocator_type>::construct(a_, static_cast<data_type *>(element()));
        }
        

        /**
            Initialization of a pointee object.
            
            @param  a   Allocator to copy.
        */
        
        node(allocator_type const & a) 
        : a_(a)
        {
            container::allocator_traits<allocator_type>::construct(a_, static_cast<data_type *>(element()));
        }

        
        template <typename... Args>
            node(Args &&... args) 
            {
                container::allocator_traits<allocator_type>::construct(a_, static_cast<data_type *>(element()), std::forward<Args>(args)...);
            }


        template <typename... Args>
            node(allocator_type const & a, Args &&... args)
            {
                container::allocator_traits<allocator_type>::construct(a_, static_cast<data_type *>(element()), std::forward<Args>(args)...);
            }

        
        /**
            Destructor.
        */
        
        virtual ~node()
        {
            container::allocator_traits<allocator_type>::destroy(a_, static_cast<data_type *>(element()));
        }


        /**
            Allocates a new @c node using the static copy of @c PoolAllocator to be used.
            
            @param  s   Disregarded.
            @return     Pointer of the new @c node.
        */

        void * operator new (size_t s)
        {
            return static_pool().allocate(1);
        }


        /**
            Allocates a new @c node .
            
            @param  s   Disregarded.
            @param  a   Copy of @c PoolAllocator to be used.
            @return     Pointer of the new @c node.
        */

        void * operator new (size_t s, allocator_type a)
        {
            return a.allocate(1);
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
