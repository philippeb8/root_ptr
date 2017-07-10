/**
    @file
    Boost detail/QNodeBase.hpp header file.

    @note
    Copyright (c) 2008-2016 Phil Bouchard <pbouchard8@gmail.com>.

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

#include <limits>
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

#include <boost/smart_ptr/detail/intrusive_list.hpp>
#include <boost/smart_ptr/detail/intrusive_stack.hpp>
#include <boost/smart_ptr/detail/classof.hpp>


namespace Qt
{

using namespace boost;

namespace smart_ptr
{

namespace detail
{


struct QNodeProxy;
struct QNodeBase;


/**
    Root class of all pointee objects.
*/

struct QNodeBase : public boost::detail::sp_counted_base
{
    /** Tag used to enlist to @c QNodeProxy::node_list_ . */
    QIntrusiveList::QNode node_tag_;

    QNodeBase()
    {
    }

    virtual ~QNodeBase()
    {
    }

protected:
    virtual void dispose()
    {
    }

    virtual void * get_deleter(std::type_info const &)
    {
        return 0;
    }

    virtual void * get_untyped_deleter()
    {
        return 0;
    }
};


} // namespace detail

} // namespace smart_ptr

#define TEMPLATEARGUMENT_DECL(z, n, text) BOOST_PP_COMMA_IF(n) T ## n
#define TEMPLATE_DECL(z, n, text) BOOST_PP_COMMA_IF(n) typename T ## n
#define ARGUMENT_DECL(z, n, text) BOOST_PP_COMMA_IF(n) T ## n const & t ## n
#define PARAMETER_DECL(z, n, text) BOOST_PP_COMMA_IF(n) t ## n

#define CONSTRUCT_NODE1(z, n, text)                                                                                             \
    template <BOOST_PP_REPEAT(n, TEMPLATE_DECL, 0)>                                                                             \
        text(BOOST_PP_REPEAT(n, ARGUMENT_DECL, 0)) : a_(static_pool())                                                          \
        {                                                                                                                       \
            container::allocator_traits<allocator_type>::construct(a_, element(), BOOST_PP_REPEAT(n, PARAMETER_DECL, 0));       \
        }

#define CONSTRUCT_NODE2(z, n, text)                                                                                             \
    template <BOOST_PP_REPEAT(n, TEMPLATE_DECL, 0)>                                                                             \
        text(allocator_type const & a, BOOST_PP_REPEAT(n, ARGUMENT_DECL, 0)) : a_(a)                                            \
        {                                                                                                                       \
            container::allocator_traits<allocator_type>::construct(a_, element(), BOOST_PP_REPEAT(n, PARAMETER_DECL, 0));       \
        }

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
        static QNode * text(allocator_type const & a, BOOST_PP_REPEAT(n, ARGUMENT_DECL, 0))                                      \
        {                                                                                                                       \
            return new (a) QNode(a, BOOST_PP_REPEAT(n, PARAMETER_DECL, 0));                                                      \
        }

#define MAKE_NODE_ALLOCATOR1(z, n, text)                                                                                        \
    template<template <typename, typename...> class Alloc, typename T, BOOST_PP_REPEAT(n, TEMPLATE_DECL, 0), typename... Args>  \
        typename QNode<T, Alloc<T, BOOST_PP_REPEAT(n, TEMPLATEARGUMENT_DECL, 0)> >::allocator_type text(Args&&... args)          \
        {                                                                                                                       \
            return typename QNode<T, Alloc<T, BOOST_PP_REPEAT(n, TEMPLATEARGUMENT_DECL, 0)> >::allocator_type(args...);          \
        }


/**
    Pointee object wrapper.
*/

template <typename T>
    class QNodeElement : public smart_ptr::detail::QNodeBase
    {
        friend class classof;

    public:
        typedef T data_type;

        /**
            Cast operator used by @c QNodePtrCommon::header() .
        */

        class classof
        {
            /** Address of the @c QNode the element belong to. */
            QNodeElement * p_;

        public:
            /**
                Casts from a @c data_type to its parent @c QNode object.

                @param  p   Address of a @c data_type member object to cast from.
            */

            classof(data_type * p)
            : p_(smart_ptr::detail::classof((data_type QNodeElement::*)(& QNodeElement::elem_), p))
            {
            }


            /**
                @return     Address of the parent @c QNode object.
            */

            operator QNodeElement * () const
            {
                return p_;
            }
        };

    protected:
        /** Pointee object.*/
        typename std::aligned_storage<sizeof(data_type), alignof(data_type)>::type elem_;
    };


template <>
    class QNodeElement<void> : public smart_ptr::detail::QNodeBase
    {
        friend class classof;

    public:
        typedef int data_type;

        /**
            Cast operator used by @c QNodePtrCommon::header() .
        */

        class classof
        {
            /** Address of the @c QNode the element belong to. */
            QNodeElement * p_;

        public:
            /**
                Casts from a @c data_type to its parent @c QNode object.

                @param  p   Address of a @c data_type member object to cast from.
            */

            classof(void * p)
            : p_(smart_ptr::detail::classof((data_type QNodeElement::*)(& QNodeElement::elem_), static_cast<data_type *>(p)))
            {
            }


            /**
                @return     Address of the parent @c QNode object.
            */

            operator QNodeElement * () const
            {
                return p_;
            }
        };

    protected:
        /** Pointee object.*/
        typename std::aligned_storage<sizeof(data_type), alignof(data_type)>::type elem_;
    };


/**
    Pointee object & allocator wrapper.

    Main class used to instanciate pointee objects and a copy of the allocator desired.
*/

template <typename T, typename PoolAllocator = pool_allocator<T> >
    class QNode : public QNodeElement<T>
    {
    public:
        typedef T data_type;
        typedef typename PoolAllocator::template rebind< QNode<T, PoolAllocator> >::other allocator_type;


        /**
            Initialization of a pointee object.

            @note Will use a static copy of the allocator which has no parameter.
        */

        QNode()
        : a_(static_pool())
        {
            container::allocator_traits<allocator_type>::construct(a_, element());
        }


        /**
            Initialization of a pointee object.

            @param  a   Allocator to copy.
        */

        QNode(allocator_type const & a)
        : a_(a)
        {
            container::allocator_traits<allocator_type>::construct(a_, element());
        }

        BOOST_PP_REPEAT_FROM_TO(1, 10, CONSTRUCT_NODE1, QNode)
        BOOST_PP_REPEAT_FROM_TO(1, 10, CONSTRUCT_NODE2, QNode)


        /**
            @return   Pointee object address.
        */

        data_type * element()
        {
            return reinterpret_cast<data_type *>(& elem_);
        }


        /**
            Destructor.
        */

        virtual ~QNode()
        {
            container::allocator_traits<allocator_type>::destroy(a_, element());
        }


        /**
            Allocates a new @c QNode using the static copy of @c PoolAllocator to be used.

            @param  s   Disregarded.
            @return     Pointer of the new @c QNode.
        */

        void * operator new (size_t /* s */)
        {
            return static_pool().allocate(1);
        }


        /**
            Allocates a new @c QNode .

            @param  s   Disregarded.
            @param  a   Copy of @c PoolAllocator to be used.
            @return     Pointer of the new @c QNode.
        */

        void * operator new (size_t s, allocator_type a)
        {
            return a.allocate(1);
        }


        /**
            Allocates a new @c QNode .

            @param  a   Copy of @c PoolAllocator to be used.
            @return     Pointer of the new @c QNode.
        */

        static QNode<T> * allocate(allocator_type const & a)
        {
            return new (a) QNode<T>(a);
        }

        BOOST_PP_REPEAT_FROM_TO(1, 10, ALLOCATE_NODE1, allocate)


        /**
            Deallocates a @c QNode from @c PoolAllocator .

            @param  p   Address of the @c QNode to deallocate.
        */

        void operator delete (void * p)
        {
            static_cast<QNode *>(p)->a_.deallocate(static_cast<QNode *>(p), 1);
        }


        /**
            Deallocates a @c QNode from @c PoolAllocator .

            @param  p   Address of the @c QNode to deallocate.
            @param  a   Copy of @c PoolAllocator to be used.
        */

        void operator delete (void * p, allocator_type a)
        {
            a.deallocate(static_cast<QNode *>(p), 1);
        }

  private:
        using QNodeElement<T>::elem_;

        /**
            Static pool.

            This is where all @c QNode are allocated when @c PoolAllocator is not
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


/**
    Pointee object & allocator wrapper.

    Main class used to instanciate pointee objects and a copy of the allocator desired.

    @note Uses @c fast_pool_allocator to instanciate the pointee object.
*/

template <typename T>
    class QFastNode : public QNode<T, fast_pool_allocator<T> >
    {
        typedef QNode<T, fast_pool_allocator<T> > base;

    public:
        using typename base::allocator_type;


        /**
            Initialization of a pointee object.

            @note Will use a static copy of the allocator with no parameter.
        */

        QFastNode()
        : base()
        {
        }


        /**
            Initialization of a pointee object.

            @param  a   Allocator to copy.
        */

        QFastNode(allocator_type const & a)
        : base(a)
        {
        }


        BOOST_PP_REPEAT_FROM_TO(1, 10, CONSTRUCT_NODE3, QFastNode)
        BOOST_PP_REPEAT_FROM_TO(1, 10, CONSTRUCT_NODE4, QFastNode)
    };


/**
    Allocates a new @c QNode using the respective allocator.

    @param  a   Allocator to be used.
    @param  args Arguments forwarded to the new object's constructor.

    @note Calls @c QNode::allocate .
*/

template<typename T, class Alloc, typename... Args>
    QNode<T, Alloc> * allocate_node(const Alloc& a, Args&&... args)
    {
        return QNode<T, Alloc>::allocate(a, args...);
    }


/**
    Instantiates an allocator.

    @param  args Arguments forwarded to the allocator's constructor.

    @note Instantiates a @c QNode::allocator_type .
*/

template<template <typename, typename...> class Alloc, typename T, typename... Args>
    typename QNode<T, Alloc<T> >::allocator_type make_node_allocator(Args&&... args)
    {
        return typename QNode<T, Alloc<T> >::allocator_type(args...);
    }


BOOST_PP_REPEAT_FROM_TO(1, 10, MAKE_NODE_ALLOCATOR1, make_QNode_allocator)


} // namespace Qt


#endif  // #ifndef BOOST_DETAIL_NODE_BASE_HPP_INCLUDED
