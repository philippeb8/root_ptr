/**
    @file
    Boost detail/node_base.hpp header file.

    @note
    Copyright (c) 2008 Phil Bouchard <pbouchard8@gmail.com>.

    Distributed under the Boost Software License, Version 1.0.

    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt

    See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#ifndef BOOST_DETAIL_NODE_BASE_HPP_INCLUDED
#define BOOST_DETAIL_NODE_BASE_HPP_INCLUDED

// MS compatible compilers support #pragma once

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <list>
#include <stack>
#include <limits>

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

#include <boost/smart_ptr/detail/intrusive_list.hpp>
#include <boost/smart_ptr/detail/intrusive_stack.hpp>
#include <boost/smart_ptr/detail/classof.hpp>


namespace boost
{

namespace smart_ptr
{

namespace detail
{


struct node_proxy;
struct node_base;


/**
    Root class of all pointee objects.
*/

struct node_base : public boost::detail::sp_counted_base
{
    intrusive_list::node node_tag_;					/**< Tag used to enlist to @c node_proxy::elements_ . */

    node_base()
    {
    }

    virtual ~node_base()
    {
    }

protected:
    virtual void dispose() 				                    {} 				/**< dunodey */
    virtual void * get_deleter( std::type_info const & ti ) { return 0; } 	/**< dunodey */
    virtual void * get_untyped_deleter() 					{ return 0; } 	/**< dunodey */
};


} // namespace detail

} // namespace smart_ptr

#define TEMPLATE_DECL(z, n, text) BOOST_PP_COMMA_IF(n) typename T ## n
#define ARGUMENT_DECL(z, n, text) BOOST_PP_COMMA_IF(n) T ## n const & t ## n
#define PARAMETER_DECL(z, n, text) BOOST_PP_COMMA_IF(n) t ## n

#define CONSTRUCT_NODE1(z, n, text)																			    \
    template <BOOST_PP_REPEAT(n, TEMPLATE_DECL, 0)>										                        \
        text(BOOST_PP_REPEAT(n, ARGUMENT_DECL, 0)) : elem_(BOOST_PP_REPEAT(n, PARAMETER_DECL, 0)) {}																										

#define CONSTRUCT_NODE2(z, n, text)                                                                                \
    template <BOOST_PP_REPEAT(n, TEMPLATE_DECL, 0)>                                                             \
        text(BOOST_PP_REPEAT(n, ARGUMENT_DECL, 0)) : base(BOOST_PP_REPEAT(n, PARAMETER_DECL, 0)) {}                                                                                                        

/**
    Object wrapper.
*/

template <typename T, typename PoolAllocator = pool_allocator<T> >
    class node : public smart_ptr::detail::node_base
    {
        typedef typename PoolAllocator::template rebind< node<T, PoolAllocator> >::other PoolType;
        
        static PoolType & static_pool() /**< Pool where all sets are allocated. */
        {
            static PoolType pool_;
            
            return pool_;
        }

        typedef T data_type;

        T elem_; 									/**< Pointee object.  @note Needs alignas<long>. */
        
    public:
        class classof;
        friend class classof;

        node() : elem_()
        {
        }

        BOOST_PP_REPEAT_FROM_TO(1, 10, CONSTRUCT_NODE1, node)


        /**
            @return		Pointee object address.
        */
        
        data_type * element() 				{ return & elem_; }

        virtual ~node()					
        { 
            dispose();
        }
        virtual void dispose()              {}

    public:
        /**
            Cast operator used by @c node_ptr_conodeon::header() .
        */
        
        class classof
        {
            node * p_;							/**< Address of the @c node the element belong to. */

        public:
            /**
                Casts from a @c data_type to its parent @c node object.
                
                @param	p	Address of a @c data_type member object to cast from.
            */
            
            classof(data_type * p) : p_(smart_ptr::detail::classof((data_type node::*)(& node::elem_), p)) {}
            
            
            /**
                @return		Address of the parent @c node object.
            */
            
            operator node * () const { return p_; }
        };

        
        /**
            Allocates a new @c node_proxy using the fast pool allocator.
            
            @param  s   Size of the @c node_proxy .
            @return     Pointer of the new memory node.
        */

        void * operator new (size_t s)
        {
            return static_pool().allocate(1);
        }
        
        
        /**
            Deallocates a @c node_proxy from the fast pool allocator.
            
            @param  p   Address of the @c node_proxy to deallocate.
        */
        
        void operator delete (void * p)
        {
            static_pool().deallocate(static_cast<node<T, PoolAllocator> *>(p), 1);
        }
    };


template <typename PoolAllocator>
    class node<void, PoolAllocator> : public smart_ptr::detail::node_base
    {
        typedef void data_type;

        long elem_; 									/**< Pointee placeholder.  @note Aligned. */

        node();

    public:
        class classof;
        friend class classof;

        data_type * element() 				{ return & elem_; }

        virtual ~node()					{}
        virtual void dispose() 				{}

        virtual void * static_deleter( std::type_info const & ti ) { return 0; }

    public:
        /**
            Cast operator used by @c node_ptr_conodeon::header() .
        */
        
        class classof
        {
            node * p_;							/**< Address of the @c node the element belong to. */

        public:
            /**
                Casts from a @c data_type to its parent @c node object.
                
                @param	p	Address of a @c data_type member object to cast from.
            */
            
            classof(data_type * p) : p_(smart_ptr::detail::classof((long node::*)(& node::elem_), static_cast<long *>(p))) {}
            
            
            /**
                @return		Address of the parent @c node object.
            */
            
            operator node * () const { return p_; }
        };
    };


template <typename T>
    class fastnode : public node<T, fast_pool_allocator<T> >
    {
    public:
        typedef node<T, fast_pool_allocator<T> > base;
        
        fastnode() : base()
        {
        }

        BOOST_PP_REPEAT_FROM_TO(1, 10, CONSTRUCT_NODE2, fastnode)
    };

    
} // namespace boost


#endif  // #ifndef BOOST_DETAIL_NODE_BASE_HPP_INCLUDED