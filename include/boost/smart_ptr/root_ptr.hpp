/*!
    \file
    \brief Boost root_ptr.hpp mainheader file.
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

#include <utility>

#ifndef BOOST_DISABLE_THREADS
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#endif

#include <boost/smart_ptr/detail/intrusive_list.hpp>
#include <boost/smart_ptr/detail/intrusive_stack.hpp>
#include <boost/smart_ptr/detail/classof.hpp>
#include <boost/smart_ptr/detail/node_ptr_base.hpp>


namespace boost
{

namespace smart_ptr
{

namespace detail
{


struct node_base;


} // namespace detail

} // namespace smart_ptr


/**
    Set header.

    Proxy object used to link a list of @c node<> blocks and a list of @c node_proxy .
*/

class node_proxy
{
    template <typename> friend class node_ptr;
    template <typename> friend class root_ptr;

    /** Stack depth. */
    int const depth_;
    
    /** Destruction sequence flag. */
    bool destroying_;

    /** List of all pointee objects belonging to a @c node_proxy . */
    mutable smart_ptr::detail::intrusive_list node_list_;

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
        Initialization of a single @c node_proxy .
    */

    node_proxy() : depth_(static_depth() ++), destroying_(false)
    {
    }


    /**
        Copy of a single @c node_proxy and unification.
    */

    node_proxy(node_proxy const & x)
    : depth_(static_depth() ++)
    , destroying_(x.destroying_)
    , node_list_(x.node_list_)
    {
    }


    /**
        Destruction of a single @c node_proxy and detaching itself from other @c node_proxy .
    */

    ~node_proxy()
    {
        reset();
        
        static_depth() --;
    }
    
    
    int depth() const
    {
        return depth_;
    }

    
private:
    bool destroying() const
    {
        return destroying_;
    }


    void destroying(bool b)
    {
        destroying_ = b;
    }


    /**
        Enlist & initialize pointee objects belonging to the same @c node_proxy .

        @param  p   Pointee object to initialize.
    */

    void init(smart_ptr::detail::node_base * p) const
    {
        node_list_.push_back(& p->node_tag_);
    }


    /**
        Get rid or delegate a series of @c node_proxy .
    */

    void reset()
    {
        using namespace smart_ptr::detail;

        // destroy cycles remaining
        destroying(true);

        for (intrusive_list::iterator<node_base, &node_base::node_tag_> m = node_list_.begin(), n = node_list_.begin(); m != node_list_.end(); m = n)
        {
            ++ n;
            delete &* m;
        }

        destroying(false);
    }
};


template <typename T>
    struct info_t
    {
        static void proxy(T const & po, node_proxy const & px)
        {
        }
    };


/**
    Deterministic region based memory manager.

    Complete memory management utility on top of standard reference counting.

    @note Must be initialized with a reference to a @c node_proxy , given by a @c root_ptr<> .
*/

template <typename T>
    class node_ptr : public smart_ptr::detail::node_ptr_base<T>
    {
        template <typename> friend class node_ptr;

        typedef smart_ptr::detail::node_ptr_base<T> base;

        using base::share;
        using base::po_;

        /** Reference to the @c node_proxy node @c node_ptr<> belongs to. */
        mutable node_proxy const * px_;


    public:
        using base::reset;

        /**
            Initialization of a pointer.

            @param  x   Reference to a @c node_proxy the pointer belongs to.
        */

        explicit node_ptr(node_proxy const & x)
        : base()
        , px_(& x)
        {
        }


        /**
            Initialization of a pointer.

            @param  x   Reference to a @c node_proxy the pointer belongs to.
            @param  p New pointee object to manage.
        */

        template <typename V, typename PoolAllocator>
            explicit node_ptr(node_proxy const & x, node<V, PoolAllocator> * p)
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
            node_ptr(node_ptr<V> const & p)
            : base(p)
            , px_(p.px_)
            {
            }


        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

            node_ptr(node_ptr<T> const & p)
            : base(p)
            , px_(p.px_)
            {
            }

            
        /**
            Assignment.

            @param  p New pointer to manage.
        */

        template <typename V>
            node_ptr & operator = (node_ptr<V> const & p)
            {
#ifndef BOOST_DISABLE_THREADS
                mutex::scoped_lock scoped_lock(node_proxy::static_mutex());
#endif
                
                // upscale the proxy of the operand
                if (px_->depth() < p.px_->depth())
                    propagate(p);

                base::operator = (p);

                return * this;
            }
            
            
        /**
            Returns associated proxy.
            
            @return     @c node_proxy part of @c root_ptr .
        */
        
        node_proxy const & proxy() 
        {
            return *px_;
        }

        
        /**
            Returns associated proxy.
            
            @return     @c node_proxy part of @c root_ptr .
        */
        
        node_proxy const & proxy() const
        {
            return *px_;
        }

        
        /**
            Sets associated proxy for the entire branch of this pointer.
        */
        
        void proxy(node_proxy const & x) const
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

        node_ptr & operator = (node_ptr<T> const & p)
        {
            return operator = <T>(p);
        }


        /**
            Assignment.

            @param  p   New pointer to manage.
        */

        template <typename V, typename PoolAllocator>
            node_ptr & operator = (node<V, PoolAllocator> * p)
            {
#ifndef BOOST_DISABLE_THREADS
                mutex::scoped_lock scoped_lock(node_proxy::static_mutex());
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
            void reset(node_ptr<V> const & p)
            {
                operator = <T>(p);
            }


        /**
            Assignment.

            @param  p   New pointer to manage.
        */

        template <typename V, typename PoolAllocator>
            void reset(node<V, PoolAllocator> * p)
            {
                operator = <T>(p);
            }


        /**
            Explicit cyclicism detection mechanism, for use @b only inside destructors.

            @return Whether the pointer inside a destructor points to an object already destroyed.

            TODO I'm not sure what this means???  return @false if there is an object to destroy?
            Need a link to the example here See root_ptr_example1.cpp for an unfinished on.

        */

        bool cyclic() const
        {
            return px_->destroying();
        }


        /**
            Destructor.
        */

        ~node_ptr()
        {
            if (cyclic())
                base::po_ = 0;
        }

#if 0 //defined(BOOST_HAS_RVALUE_REFS)
    public:
        node_ptr(node_ptr<T> && p): base(p), px_(p.px_)
        {
            p.po_ = 0;
        }

        template<class Y>
            node_ptr(node_ptr<Y> && p): base(p), px_(p.px_)
            {
                p.po_ = 0;
            }
#endif

    private:
        template <typename V>
            void propagate(node_ptr<V> const & p) const
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

    @note Needs to be instanciated for the use of further @c node_ptr<> .
*/

template <typename T>
    class root_ptr : public node_proxy, public node_ptr<T>
    {
    public:

        /**
            Assignment.
        */

        using node_ptr<T>::reset;


        /**
            Initialization of a pointer.
        */

        root_ptr()
        : node_proxy()
        , node_ptr<T>(* static_cast<node_proxy *>(this))
        {
        }


        /**
            Initialization of a pointer.

            @param  p   New pointer to manage.
        */

        root_ptr(root_ptr const & p)
        : node_proxy(p)
        , node_ptr<T>(static_cast<node_ptr<T> const &>(p))
        {
        }


        /**
            Initialization of a pointer.

            @param  p   New pointee object to manage.
        */

        template <typename V, typename PoolAllocator>
            root_ptr(node<V, PoolAllocator> * p)
            : node_ptr<T>(*this, p)
            {
            }


        /**
            Assignment.

            @param  p   New pointer to manage.
        */

        template <typename V>
            root_ptr & operator = (node_ptr<V> const & p)
            {
                return static_cast<root_ptr &>(node_ptr<T>::operator = (p));
            }


        /**
            Assignment.

            @param  p   New pointer to manage.
        */

        root_ptr & operator = (node_ptr<T> const & p)
        {
            return operator = <T>(p);
        }


        /**
            Assignment.

            @param  p   New pointer to manage.
        */

        template <typename V>
            root_ptr & operator = (root_ptr<V> const & p)
            {
                return static_cast<root_ptr &>(node_ptr<T>::operator = (p));
            }


        /**
            Assignment.

            @param  p   New pointer to manage.
        */

        root_ptr & operator = (root_ptr<T> const & p)
        {
            return operator = <T>(p);
        }


        /**
            Assignment.

            @param  p   New pointee object to manage.
        */

        template <typename V, typename PoolAllocator>
            root_ptr<T> & operator = (node<V, PoolAllocator> * p)
            {
                return static_cast<root_ptr<T> &>(node_ptr<T>::operator = (p));
            }
            
        
        /**
            Cast operation helper.
            
            @return     @c node_proxy part of @c root_ptr .
        */
        
        node_proxy & proxy() 
        {
            return *this;
        }

        
        /**
            Cast operation helper.
            
            @return     @c node_proxy part of @c root_ptr .
        */
        
        node_proxy const & proxy() const
        {
            return *this;
        }
    };


/**
    Instanciates a new @c root_ptr<> .
*/

template <typename V, typename... Args, typename PoolAllocator = pool_allocator<V> >
    root_ptr<V> make_root(Args... args)
    {
        return root_ptr<V>(new node<V, PoolAllocator>(args...));
    }


/**
    Instanciates a new @c node_ptr<> .

    @param  x   Reference to a @c node_proxy the pointer belongs to.
*/

template <typename V, typename... Args, typename PoolAllocator = pool_allocator<V> >
    node_ptr<V> make_node(node_proxy const & x, Args... args)
    {
        return node_ptr<V>(x, new node<V, PoolAllocator>(args...));
    }


/**
    Instanciates a new @c root_ptr<> .

    @note Uses @c fast_pool_allocator to instanciate the pointee object.
*/

template <typename V, typename... Args, typename PoolAllocator = pool_allocator<V> >
    root_ptr<V> make_fastroot(Args... args)
    {
        return root_ptr<V>(new fastnode<V>(args...));
    }


/**
    Instanciates a new @c node_ptr<> .

    @param  x   Reference to a @c node_proxy the pointer belongs to.

    @note Uses @c fast_pool_allocator to instanciate the pointee object.
*/

template <typename V, typename... Args, typename PoolAllocator = pool_allocator<V> >
    node_ptr<V> make_fastnode(node_proxy const & x, Args... args)
    {
        return node_ptr<V>(x, new fastnode<V>(args...));
    }

    
/**
    Comparison operator.

    @param  a1  Operand 1.
    @param  a2  Operand 2.
*/

template <typename T>
    bool operator == (node_ptr<T> const &a1, node_ptr<T> const &a2)
    {
        return a1.get() == a2.get();
    }


/**
    Comparison operator.

    @param  a1  Operand 1.
    @param  a2  Operand 2.
*/

template <typename T>
    bool operator != (node_ptr<T> const &a1, node_ptr<T> const &a2)
    {
        return a1.get() != a2.get();
    }


} // namespace boost


#if defined(_MSC_VER)
#pragma warning( pop )
#endif


#endif // #ifndef BOOST_NODE_PTR_INCLUDED
