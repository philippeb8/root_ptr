/**
    @file
    Boost detail/block_base.hpp header file.

    @note
    Copyright (c) 2008 Phil Bouchard <pbouchard8@gmail.com>.

    Distributed under the Boost Software License, Version 1.0.

    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt

    See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#ifndef BOOST_DETAIL_BLOCK_BASE_HPP_INCLUDED
#define BOOST_DETAIL_BLOCK_BASE_HPP_INCLUDED

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

#include <boost/detail/intrusive_list.hpp>
#include <boost/detail/intrusive_stack.hpp>
#include <boost/detail/classof.hpp>
#include <boost/detail/system_pool.hpp>


namespace boost
{

namespace detail
{

namespace bp
{


struct block_proxy;
struct block_base;


/**
    Allocator wrapper tracking allocations.
    
    Pool where all pointee objects are allocated and tracks memory blocks for later enlisting & marking the @c block_proxy the pointee object belongs to.
*/

template <typename UserPool>
    struct pool
    {
        typedef UserPool pool_t;


        /**
            Tells whether a pointer is part of the pool or not.
            
            @param	p	Pointer to object.
            @return		Belongs to the pool.
        */
        
        static bool is_from(void const * p)
        {
            return pool_t::is_from(const_cast<void *>(p));
        }
        
        
        
        /**
            Pointee object allocator and stacking of the newly allocated memory boundary.
            
            @param	s	Size of the memory block to allocate.
            @return		Address of the newly allocated block.
        */
        
        static void * allocate(std::size_t s)
        {
            return pool_t::ordered_malloc(s);;
        }

        
        /**
            Pointee object deallocator and removal of the boundaries that were allocated before the pointer was allocated.
            
            @param	p	Address of the memory block to deallocate.
            @param	s	Size of the memory block.
        */
        
        static void deallocate(void * p, std::size_t s)
        {
            pool_t::ordered_free(p, s);
        }
    };


/**
    Root class of all pointee objects.
*/

struct block_base : public sp_counted_base
{
    bool init_;										/**< Flag marking initialization of the pointee object to its @c block_proxy . */

    intrusive_list inits_;							/**< List of all pointee objects that will later need to be initlialized to a specific @c block_proxy .*/

    intrusive_list::node block_tag_;					/**< Tag used to enlist to @c block_proxy::elements_ . */
    intrusive_list::node init_tag_;					/**< Tag used to enlist to @c block_base::inits_ . */

    block_base() : init_(false)
    {
        inits_.push_back(& init_tag_);
    }

protected:
    virtual void dispose() 				                    {} 				/**< dublocky */
    virtual void * get_deleter( std::type_info const & ti ) { return 0; } 	/**< dublocky */
    virtual void * get_untyped_deleter() 					{ return 0; } 	/**< dublocky */
};


#define TEMPLATE_DECL(z, n, text) BOOST_PP_COMMA_IF(n) typename T ## n
#define ARGUMENT_DECL(z, n, text) BOOST_PP_COMMA_IF(n) T ## n const & t ## n
#define PARAMETER_DECL(z, n, text) BOOST_PP_COMMA_IF(n) t ## n

#define CONSTRUCT_BLOCK(z, n, text)																			    \
    template <BOOST_PP_REPEAT(n, TEMPLATE_DECL, 0)>										                        \
        text(BOOST_PP_REPEAT(n, ARGUMENT_DECL, 0)) : elem_(BOOST_PP_REPEAT(n, PARAMETER_DECL, 0)) {}																										

/**
    Object wrapper.
*/

template <typename T, typename UserPool = system_pool<system_pool_tag, sizeof(char)> >
    class block : public block_base
    {
        typedef T data_type;

        T elem_; 									/**< Pointee object.  @note Needs alignas<long>. */
        
    public:
        class classof;
        friend class classof;

        block() : elem_() 
        {
        }

        BOOST_PP_REPEAT_FROM_TO(1, 10, CONSTRUCT_BLOCK, block)


        /**
            @return		Pointee object address.
        */
        
        data_type * element() 				{ return & elem_; }

        virtual ~block()					
        { 
            dispose();
        }
        virtual void dispose()              {}

    public:
        /**
            Cast operator used by @c block_ptr_coblockon::header() .
        */
        
        class classof
        {
            block * p_;							/**< Address of the @c block the element belong to. */

        public:
            /**
                Casts from a @c data_type to its parent @c block object.
                
                @param	p	Address of a @c data_type member object to cast from.
            */
            
            classof(data_type * p) : p_(bp::classof((data_type block::*)(& block::elem_), p)) {}
            
            
            /**
                @return		Address of the parent @c block object.
            */
            
            operator block * () const { return p_; }
        };

        
        /**
            Allocates a new @c block using the pool.
            
            @param	s	Size of the @c block .
            @return		Pointer of the new memory block.
        */
        
        void * operator new (size_t s)
        {
            return pool<UserPool>::allocate(s);
        }
        

        /**
            Deallocates a @c block from the pool.
            
            @param	p	Address of the @c block to deallocate.
        */
        
        void operator delete (void * p)
        {
            pool<UserPool>::deallocate(p, sizeof(block));
        }
    };


template <typename UserPool>
    class block<void, UserPool> : public block_base
    {
        typedef void data_type;

        long elem_; 									/**< Pointee placeholder.  @note Aligned. */

        block();

    public:
        class classof;
        friend class classof;

        data_type * element() 				{ return & elem_; }

        virtual ~block()					{}
        virtual void dispose() 				{}

        virtual void * static_deleter( std::type_info const & ti ) { return 0; }

    public:
        /**
            Cast operator used by @c block_ptr_coblockon::header() .
        */
        
        class classof
        {
            block * p_;							/**< Address of the @c block the element belong to. */

        public:
            /**
                Casts from a @c data_type to its parent @c block object.
                
                @param	p	Address of a @c data_type member object to cast from.
            */
            
            classof(data_type * p) : p_(bp::classof((long block::*)(& block::elem_), static_cast<long *>(p))) {}
            
            
            /**
                @return		Address of the parent @c block object.
            */
            
            operator block * () const { return p_; }
        };
    };


} // namespace bp

} // namespace detail

} // namespace boost


#endif  // #ifndef BOOST_DETAIL_BLOCK_BASE_HPP_INCLUDED
