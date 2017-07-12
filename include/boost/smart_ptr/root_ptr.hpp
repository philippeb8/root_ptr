/*!
    \file
    \brief Boost QRootPtr.hpp mainheader file.
*/
/*
    Copyright 2008-2016 Phil Bouchard.

    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt
*/

#ifndef BOOST_NODE_PTR_INCLUDED
#define BOOST_NODE_PTR_INCLUDED

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4355 )

#include <new.h>
#endif

#ifndef BOOST_DISABLE_THREADS
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#endif

#include <boost/smart_ptr/detail/intrusive_list.hpp>
#include <boost/smart_ptr/detail/intrusive_stack.hpp>
#include <boost/smart_ptr/detail/classof.hpp>
#include <boost/smart_ptr/detail/node_ptr_base.hpp>


namespace Qt
{

using namespace boost;

namespace smart_ptr
{

namespace detail
{


struct QNodeBase;


} // namespace detail

} // namespace smart_ptr


/**
    Set header.

    Proxy object used to link a list of @c QNode<> blocks and a list of @c QNodeProxy .
*/

class QNodeProxy
{
    template <typename> friend class QNodePtr;
    template <typename> friend class QRootPtr;

    /** Stack depth. */
    int const depth_;
    
    /** Destruction sequence flag. */
    bool destroying_;

    /** List of all pointee objects belonging to a @c QNodeProxy . */
    mutable smart_ptr::detail::QIntrusiveList node_list_;

#ifndef BOOST_DISABLE_THREADS
    /** Main global mutex used for thread safety */
    static mutex & static_mutex()
    {
        static mutex mutex_;

        return mutex_;
    }
#endif

    /** Main global mutex used for thread safety */
    static int & static_depth()
    {
        static int depth_ = 0;

        return depth_;
    }

public:
    /**
        Initialization of a single @c QNodeProxy .
    */

    QNodeProxy() : depth_(static_depth() ++), destroying_(false)
    {
    }


    /**
        Copy of a single @c QNodeProxy and unification.
    */

    QNodeProxy(QNodeProxy const & x)
    : depth_(static_depth() ++)
    , destroying_(x.destroying_)
    , node_list_(x.node_list_)
    {
    }


    /**
        Destruction of a single @c QNodeProxy and detaching itself from other @c QNodeProxy .
    */

    ~QNodeProxy()
    {
        reset();
        
        static_depth() --;
    }


private:
    int depth() const
    {
        return depth_;
    }

    
    bool destroying() const
    {
        return destroying_;
    }


    void destroying(bool b)
    {
        destroying_ = b;
    }


    /**
        Enlist & initialize pointee objects belonging to the same @c QNodeProxy .

        @param  p   Pointee object to initialize.
    */

    void init(smart_ptr::detail::QNodeBase * p) const
    {
        node_list_.push_back(& p->node_tag_);
    }


    /**
        Get rid or delegate a series of @c QNodeProxy .
    */

    void reset()
    {
        using namespace smart_ptr::detail;

        // destroy cycles remaining
        destroying(true);

        for (QIntrusiveList::iterator<QNodeBase, &QNodeBase::node_tag_> m = node_list_.begin(), n = node_list_.begin(); m != node_list_.end(); m = n)
        {
            ++ n;
            delete &* m;
        }

        destroying(false);
    }
};


#define TEMPLATE_DECL(z, n, text) BOOST_PP_COMMA_IF(n) typename T ## n
#define ARGUMENT_DECL(z, n, text) BOOST_PP_COMMA_IF(n) T ## n const & t ## n
#define PARAMETER_DECL(z, n, text) BOOST_PP_COMMA_IF(n) t ## n

#define CONSTRUCT_MAKE_ROOT1(z, n, text)                                                                                \
    template <typename V, BOOST_PP_REPEAT(n, TEMPLATE_DECL, 0), typename PoolAllocator = pool_allocator<V> >            \
        QRootPtr<V> text(BOOST_PP_REPEAT(n, ARGUMENT_DECL, 0))                                                          \
        {                                                                                                               \
            return QRootPtr<V>(new QNode<V, PoolAllocator>(BOOST_PP_REPEAT(n, PARAMETER_DECL, 0)));                      \
        }

#define CONSTRUCT_MAKE_NODE2(z, n, text)                                                                                \
    template <typename V, BOOST_PP_REPEAT(n, TEMPLATE_DECL, 0), typename PoolAllocator = pool_allocator<V> >            \
        QNodePtr<V> text(QNodeProxy const & x, BOOST_PP_REPEAT(n, ARGUMENT_DECL, 0))                                    \
        {                                                                                                               \
            return QNodePtr<V>(x, new QNode<V, PoolAllocator>(BOOST_PP_REPEAT(n, PARAMETER_DECL, 0)));                   \
        }

#define CONSTRUCT_MAKE_ROOT3(z, n, text)                                                                                \
    template <typename V, BOOST_PP_REPEAT(n, TEMPLATE_DECL, 0), typename PoolAllocator = pool_allocator<V> >            \
        QRootPtr<V> text(BOOST_PP_REPEAT(n, ARGUMENT_DECL, 0))                                                          \
        {                                                                                                               \
            return QRootPtr<V>(new QFastNode<V>(BOOST_PP_REPEAT(n, PARAMETER_DECL, 0)));                                 \
        }

#define CONSTRUCT_MAKE_NODE4(z, n, text)                                                                                \
    template <typename V, BOOST_PP_REPEAT(n, TEMPLATE_DECL, 0), typename PoolAllocator = pool_allocator<V> >            \
        QNodePtr<V> text(QNodeProxy const & x, BOOST_PP_REPEAT(n, ARGUMENT_DECL, 0))                                    \
        {                                                                                                               \
            return QNodePtr<V>(x, new QFastNode<V>(BOOST_PP_REPEAT(n, PARAMETER_DECL, 0)));                              \
        }


template <typename T>
    struct info_t
    {
        static void proxy(T const & po, QNodeProxy const & px)
        {
        }
    };
    
/**
    Deterministic region based memory manager.

    Complete memory management utility on top of standard reference counting.

    @note Must be initialized with a reference to a @c QNodeProxy , given by a @c QRootPtr<> .
*/

template <typename T>
    class QNodePtr : public smart_ptr::detail::QNodePtrBase<T>
    {
        template <typename> friend class QNodePtr;

        typedef smart_ptr::detail::QNodePtrBase<T> base;

        using base::share;
        using base::po_;

        /** Reference to the @c QNodeProxy QNode @c QNodePtr<> belongs to. */
        mutable QNodeProxy const * px_;

    public:
        using base::reset;

        /**
            Initialization of a pointer.

            @param  x   Reference to a @c QNodeProxy the pointer belongs to.
        */

        explicit QNodePtr(QNodeProxy const & x)
        : base()
        , px_(& x)
        {
        }


        /**
            Initialization of a pointer.

            @param  x   Reference to a @c QNodeProxy the pointer belongs to.
            @param  p New pointee object to manage.
        */

        template <typename V, typename PoolAllocator>
            explicit QNodePtr(QNodeProxy const & x, QNode<V, PoolAllocator> * p)
            : base(p)
            , px_(& x)
            {
                px_->init(p);
            }


    public:
        typedef T value_type;


        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            QNodePtr(QNodePtr<V> const & p)
            : base(p)
            , px_(p.px_)
            {
            }


        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

            QNodePtr(QNodePtr<T> const & p)
            : base(p)
            , px_(p.px_)
            {
            }


        /**
            Assignment.

            @param  p New pointer to manage.
        */

        template <typename V>
            QNodePtr & operator = (QNodePtr<V> const & p)
            {
#ifndef BOOST_DISABLE_THREADS
                mutex::scoped_lock scoped_lock(QNodeProxy::static_mutex());
#endif
                
                // upscale the proxy of the operand
                if (px_->depth() < p.px_->depth())
                    propagate(p);
                
                base::operator = (p);

                return * this;
            }
            
            
        /**
            Cast operation helper.
            
            @return     @c QNodeProxy part of @c QRootPtr .
        */
        
        QNodeProxy & proxy() 
        {
            return *px_;
        }

        
        /**
            Cast operation helper.
            
            @return     @c QNodeProxy part of @c QRootPtr .
        */
        
        QNodeProxy const & proxy() const
        {
            return *px_;
        }

        
        void proxy(QNodeProxy const & x) const
        {
            if (px_ != & x)
            {
                px_ = & x;
                
                propagate(* this);
            }
        }


        /**
            Assignment.

            @param  p New pointer to manage.
        */

        QNodePtr & operator = (QNodePtr<T> const & p)
        {
            return operator = <T>(p);
        }


        /**
            Assignment.

            @param  p   New pointer to manage.
        */

        template <typename V, typename PoolAllocator>
            QNodePtr & operator = (QNode<V, PoolAllocator> * p)
            {
#ifndef BOOST_DISABLE_THREADS
                mutex::scoped_lock scoped_lock(QNodeProxy::static_mutex());
#endif
                
                px_->init(p);

                base::operator = (p);

                return * this;
            }


        /**
            Assignment.

            @param  p   New pointer to manage.
        */

        template <typename V>
            void reset(QNodePtr<V> const & p)
            {
                operator = <T>(p);
            }


        /**
            Assignment.

            @param  p   New pointer to manage.
        */

        template <typename V, typename PoolAllocator>
            void reset(QNode<V, PoolAllocator> * p)
            {
                operator = <T>(p);
            }


        /**
            Explicit cyclicism detection mechanism, for use @b only inside destructors.

            @return Whether the pointer inside a destructor points to an object already destroyed.

            TODO I'm not sure what this means???  return @false if there is an object to destroy?
            Need a link to the example here See QRootPtr_example1.cpp for an unfinished on.

        */

        bool cyclic() const
        {
            return px_->destroying();
        }


        /**
            Destructor.
        */

        ~QNodePtr()
        {
            if (cyclic())
                base::po_ = 0;
        }

#if 0 //defined(BOOST_HAS_RVALUE_REFS)
    public:
        QNodePtr(QNodePtr<T> && p): base(p.po_), px_(p.px_)
        {
            p.po_ = 0;
        }

        template<class Y>
            QNodePtr(QNodePtr<Y> && p): base(p.po_), px_(p.px_)
            {
                p.po_ = 0;
            }

        QNodePtr<T> & operator = (QNodePtr<T> && p)
        {
            std::swap(po_, p.po_);
            std::swap(px_, p.px_);

            return *this;
        }

        template<class Y>
            QNodePtr & operator = (QNodePtr<Y> && p)
            {
                std::swap(po_, p.po_);
                std::swap(px_, p.px_);

                return *this;
            }
#endif

    private:
        template <typename V>
            void propagate(QNodePtr<V> const & p) const
            {
                if (p.po_)
                {
                    p.header()->node_tag_.erase();
                    info_t<V>::proxy(* p.po_, * px_);
                    px_->init(p.header());
                }
            }
    };


/**
    Deterministic region based memory manager.

    Complete memory management utility on top of standard reference counting.

    @note Needs to be instanciated for the use of further @c QNodePtr<> .
*/

template <typename T>
    class QRootPtr : public QNodeProxy, public QNodePtr<T>
    {
    public:

        /**
            Assignment.
        */

        using QNodePtr<T>::reset;


        /**
            Initialization of a pointer.
        */

        QRootPtr()
        : QNodeProxy()
        , QNodePtr<T>(* static_cast<QNodeProxy *>(this))
        {
        }


        /**
            Initialization of a pointer.

            @param  p   New pointer to manage.
        */

        QRootPtr(QRootPtr const & p)
        : QNodeProxy(p)
        , QNodePtr<T>(static_cast<QNodePtr<T> const &>(p))
        {
        }


        /**
            Initialization of a pointer.

            @param  p   New pointee object to manage.
        */

        template <typename V, typename PoolAllocator>
            QRootPtr(QNode<V, PoolAllocator> * p)
            : QNodePtr<T>(*this, p)
            {
            }


        /**
            Assignment.

            @param  p   New pointer to manage.
        */

        template <typename V>
            QRootPtr & operator = (QNodePtr<V> const & p)
            {
                return static_cast<QRootPtr &>(QNodePtr<T>::operator = (p));
            }


        /**
            Assignment.

            @param  p   New pointer to manage.
        */

        QRootPtr & operator = (QNodePtr<T> const & p)
        {
            return operator = <T>(p);
        }


        /**
            Assignment.

            @param  p   New pointer to manage.
        */

        template <typename V>
            QRootPtr & operator = (QRootPtr<V> const & p)
            {
                return static_cast<QRootPtr &>(QNodePtr<T>::operator = (p));
            }


        /**
            Assignment.

            @param  p   New pointer to manage.
        */

        QRootPtr & operator = (QRootPtr<T> const & p)
        {
            return operator = <T>(p);
        }


        /**
            Assignment.

            @param  p   New pointee object to manage.
        */

        template <typename V, typename PoolAllocator>
            QRootPtr<T> & operator = (QNode<V, PoolAllocator> * p)
            {
                return static_cast<QRootPtr<T> &>(QNodePtr<T>::operator = (p));
            }
            
        
        /**
            Cast operation helper.
            
            @return     @c QNodeProxy part of @c QRootPtr .
        */
        
        QNodeProxy & proxy() 
        {
            return *this;
        }

        
        /**
            Cast operation helper.
            
            @return     @c QNodeProxy part of @c QRootPtr .
        */
        
        QNodeProxy const & proxy() const
        {
            return *this;
        }
    };


/**
    Instanciates a new @c QRootPtr<> .
*/

template <typename V, typename PoolAllocator = pool_allocator<V> >
    QRootPtr<V> make_root()
    {
        return QRootPtr<V>(new QNode<V, PoolAllocator>());
    }


/**
    Instanciates a new @c QNodePtr<> .

    @param  x   Reference to a @c QNodeProxy the pointer belongs to.
*/

template <typename V, typename PoolAllocator = pool_allocator<V> >
    QNodePtr<V> make_node(QNodeProxy const & x)
    {
        return QNodePtr<V>(x, new QNode<V, PoolAllocator>());
    }


/**
    Instanciates a new @c QRootPtr<> .

    @note Uses @c fast_pool_allocator to instanciate the pointee object.
*/

template <typename V, typename PoolAllocator = pool_allocator<V> >
    QRootPtr<V> make_fastroot()
    {
        return QRootPtr<V>(new QFastNode<V>());
    }


/**
    Instanciates a new @c QNodePtr<> .

    @param  x   Reference to a @c QNodeProxy the pointer belongs to.

    @note Uses @c fast_pool_allocator to instanciate the pointee object.
*/

template <typename V, typename PoolAllocator = pool_allocator<V> >
    QNodePtr<V> make_fastnode(QNodeProxy const & x)
    {
        return QNodePtr<V>(x, new QFastNode<V>());
    }


/**
    Comparison operator.

    @param  a1  Operand 1.
    @param  a2  Operand 2.
*/

template <typename T>
    bool operator == (QNodePtr<T> const &a1, QNodePtr<T> const &a2)
    {
        return a1.get() == a2.get();
    }


/**
    Comparison operator.

    @param  a1  Operand 1.
    @param  a2  Operand 2.
*/

template <typename T>
    bool operator != (QNodePtr<T> const &a1, QNodePtr<T> const &a2)
    {
        return a1.get() != a2.get();
    }


BOOST_PP_REPEAT_FROM_TO(1, 10, CONSTRUCT_MAKE_ROOT1, make_root)
BOOST_PP_REPEAT_FROM_TO(1, 10, CONSTRUCT_MAKE_NODE2, make_node)
BOOST_PP_REPEAT_FROM_TO(1, 10, CONSTRUCT_MAKE_ROOT3, make_fastroot)
BOOST_PP_REPEAT_FROM_TO(1, 10, CONSTRUCT_MAKE_NODE4, make_fastnode)


} // namespace Qt


#if defined(_MSC_VER)
#pragma warning( pop )
#endif


#endif // #ifndef BOOST_NODE_PTR_INCLUDED
