/*!
    \file
    \brief Boost node_ptr.hpp mainheader file.
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

#include <vector>
#include <utility>
#include <initializer_list>

#ifndef BOOST_DISABLE_THREADS
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#endif

#include <boost/stacktrace.hpp>
#include <boost/log/trivial.hpp>
#include <boost/smart_ptr/detail/intrusive_list.hpp>
#include <boost/smart_ptr/detail/intrusive_stack.hpp>
#include <boost/smart_ptr/detail/classof.hpp>
#include <boost/smart_ptr/detail/node_ptr_base.hpp>
#include <boost/tti/has_static_member_function.hpp>


/**
    Bug workaround in the C++ stdlib.
 */

namespace std
{
    template <>
        struct vector<void> : public vector<char>
        {
        };
}


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
    template <typename> friend class node_ptr;

    /** Stack depth. */
    size_t const depth_;
    
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

protected:
    /** Main global mutex used for thread safety */
    static size_t & static_depth()
    {
        static size_t depth_ = 0;

        return depth_;
    }

public:
    /**
        Initialization of a single @c node_proxy .
    */

    node_proxy(size_t depth = 0) : depth_(depth), destroying_(false)
    {
    }


    /**
        Disabled copy constructor.
    */

    node_proxy(node_proxy const & x) = delete;


    /**
        Destruction of a single @c node_proxy and detaching itself from other @c node_proxy .
    */

    ~node_proxy()
    {
        reset();
    }
    
    
    size_t depth() const
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
            //BOOST_LOG_TRIVIAL(info) << "<cycle>:" << boost::stacktrace::stacktrace(3, 1);
            
            ++ n;
            delete &* m;
        }

        destroying(false);
    }
};


class stack_node_proxy : public node_proxy
{
public:
    /**
        Initialization of a single @c stack_node_proxy .
    */

    stack_node_proxy() : node_proxy(++ static_depth())
    {
    }


    /**
        Destruction of a single @c node_proxy and detaching itself from other @c node_proxy .
    */

    ~stack_node_proxy()
    {
        -- static_depth();
    }
};


template <typename T>
    class root_ptr;

template <typename T, size_t S>
    class root_array;

    
BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION(__proxy)


template <typename T, bool = has_static_member_function___proxy<T, T const & (node_proxy const &, T const &)>::value>
    struct proxy
    {
        inline T const & operator () (node_proxy const & __y, T const & po) 
        { 
            return po; 
        }
    };

template <typename T, size_t S>
    struct proxy<T [S], false>
    {
        inline T const (& operator () (node_proxy const & __y, T const (& po)[S]))[S]
        {
            for (T const * i = po; i != po + S; ++ i)
                proxy<T>()(__y, * i);
            
            return po;
        }
    };

template <typename T>
    struct proxy<std::vector<T>, false>
    {
        inline std::vector<T> const & operator () (node_proxy const & __y, std::vector<T> const & po)
        {
            for (typename std::vector<T>::const_iterator i = po.begin(); i != po.end(); ++ i)
                proxy<T>()(__y, * i);
            
            return po;
        }
    };

template <>
    struct proxy<std::vector<void>, false>
    {
        inline std::vector<void> const & operator () (node_proxy const & __y, std::vector<void> const & po)
        {
            return po;
        }
    };
    
template <typename T>
    struct proxy<root_ptr<T>, false>
    {
        inline root_ptr<T> const & operator () (node_proxy const & x, root_ptr<T> const & po)
        {
            po.proxy(x);
            
            return po;
        }
    };

template <typename T, size_t S>
    struct proxy<root_array<T, S>, false>
    {
        inline root_array<T, S> const & operator () (node_proxy const & x, root_array<T, S> const & po)
        {
            po.proxy(x);
            
            return po;
        }
    };

template <typename T>
    struct proxy<T, true>
    {
        inline T const & operator () (node_proxy const & __y, T const & po) 
        { 
            T::__proxy(__y, po);
            
            return po;
        }
    };
    
template <typename T>
    inline T const & make_proxy(node_proxy const & __y, T const & po)
    {
        return proxy<T>()(__y, po);
    }
    
    
/**
    Deterministic region based memory manager.

    Complete memory management utility on top of standard reference counting.

    @note Must be initialized with a reference to a @c node_proxy , given by a @c node_ptr<> .
*/

template <typename T>
    class node_ptr : public smart_ptr::detail::node_ptr_common<T>
    {
        template <typename> friend class node_ptr;

        typedef smart_ptr::detail::node_ptr_common<T> base;

        
    protected:
        using base::po_;
        using base::share;
        using base::header;

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
            @param  p New pointee object to iterate.
        */

        template <typename V>
            explicit node_ptr(node_proxy const & x, V * p)
            : base(p)
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

        template <typename V>
            node_ptr(node_ptr<V> const & p, smart_ptr::detail::static_cast_tag const & t)
            : base(p, t)
            , px_(p.px_)
            {
            }


        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            node_ptr(node_ptr<V> const & p, smart_ptr::detail::dynamic_cast_tag const & t)
            : base(p, t)
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

            @param  p   New pointer expected to be null.
        */

        node_ptr & operator = (T * p)
        {
            if (! p)
                reset(nullptr);
            else
                throw std::out_of_range("illegal pointer");                

            return * this;
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

            @param  p New pointer to manage.
        */

        template <typename V>
            node_ptr & operator = (node_ptr<V> const & p)
            {
#ifndef BOOST_DISABLE_THREADS
                mutex::scoped_lock scoped_lock(node_proxy::static_mutex());
#endif
                
                if (px_->depth() < p.px_->depth())
                    p.proxy(* px_);

                base::operator = (p);

                if (p.px_->depth() < px_->depth())
                    proxy(* p.px_);
                
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
            Returns associated proxy.
            
            @return     @c node_proxy part of @c node_ptr .
        */
        
        node_proxy const & proxy() 
        {
            return * px_;
        }

        
        /**
            Returns associated proxy.
            
            @return     @c node_proxy part of @c node_ptr .
        */
        
        node_proxy const & proxy() const
        {
            return * px_;
        }

        
        /**
            Sets associated proxy for the entire branch of this pointer.
        */
        
        void proxy(node_proxy const & x) const
        {
            if (x.depth() < px_->depth())
            {
                px_ = & x;

                if (po_)
                {
                    header()->node_tag_.erase();
                    px_->init(header());
                    boost::proxy<T>()(x, * po_);
                }
            }
        }


        /**
            Explicit cyclicism detection mechanism, for use @b only inside destructors.

            @return Whether the pointer inside a destructor points to an object already destroyed.

            TODO I'm not sure what this means???  return @false if there is an object to destroy?
            Need a link to the example here See node_ptr_example1.cpp for an unfinished on.

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
                po_ = 0;
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
    };


template <typename T>
    class root_ptr;
    

template <>
    class root_ptr<std::vector<void>> : public node_ptr<std::vector<void>>
    {
        template <typename> friend class root_ptr;
        
        typedef node_ptr<std::vector<void>> base;
        
    protected:
        using base::po_;
        using base::header;
        using base::cyclic;

        /** Iterator. */
        void * pi_;
        char const * const pn_;

    public:
        typedef typename base::value_type value_type;

        using base::proxy;


        root_ptr(root_ptr const & p)
        : base(p)
        , pi_(p.pi_)
        , pn_(p.pn_)
        {
        }

        template <typename V>
            root_ptr(root_ptr<std::vector<V>> const & p)
            : base(p)
            , pi_(p.pi_)
            , pn_(p.pn_)
            {
            }
        
        root_ptr(node_proxy const & x, char const * n) 
        : base(x)
        , pi_(nullptr)
        , pn_(n)
        {
        }

        root_ptr(node_proxy const & x, char const * n, void const * p)
        : base(x)
        , pi_(const_cast<void *>(p))
        , pn_(n)
        {
        }

        /** Integer type casts. */
        
        template <typename V>
            root_ptr(node_proxy const & x, char const * n, V p)
            : base(x)
            , pi_(reinterpret_cast<void *>(p))
            , pn_(n)
            {
            }

        template <typename V, typename PoolAllocator>
            root_ptr(node_proxy const & x, char const * n, node<V, PoolAllocator> * p) 
            : base(x, p)
            , pi_(p->element()->data())
            , pn_(n)
            {
            }

            root_ptr(node_proxy const & x, char const * n, root_ptr const & p) 
            : base(p)
            , pi_(p.pi_)
            , pn_(n)
            {
            }

        template <typename V>
            root_ptr(node_proxy const & x, char const * n, root_ptr<std::vector<V>> const & p) 
            : base(p)
            , pi_(p.pi_)
            , pn_(n)
            {
            }

        char const * const name() const
        {
            return pn_;
        }
        
        root_ptr & operator = (void const * p)
        {
            pi_ = const_cast<void *>(p);
            
            return * this;
        }
        
        template <typename V>
            root_ptr & operator = (V * p)
            {
                pi_ = p;
                
                return * this;
            }

            root_ptr & operator = (root_ptr const & p)
            {
                pi_ = p.pi_;
                
                return static_cast<root_ptr &>(base::operator = (p));
            }

        template <typename V>
            root_ptr & operator = (root_ptr<std::vector<V>> const & p)
            {
                pi_ = p.pi_;
                
                return static_cast<root_ptr &>(base::operator = (p));
            }

        operator bool () const
        {
            return pi_ != 0;
        }

        bool operator ! () const
        {
            return pi_ == 0;
        }

        operator void * ()
        {
            return pi_;
        }

        operator void const * () const
        {
            return pi_;
        }
        
        template <typename V>
            bool operator == (root_ptr<std::vector<V>> const & o) const
            {
                return pi_ == o.pi_;
            }

        template <typename V>
            bool operator != (root_ptr<std::vector<V>> const & o) const
            {
                return pi_ != o.pi_;
            }

        template <typename V>
            bool operator < (root_ptr<std::vector<V>> const & o) const
            {
                return pi_ < o.pi_;
            }

        template <typename V>
            bool operator > (root_ptr<std::vector<V>> const & o) const
            {
                return pi_ > o.pi_;
            }

        template <typename V>
            bool operator <= (root_ptr<std::vector<V>> const & o) const
            {
                return pi_ <= o.pi_;
            }

        template <typename V>
            bool operator >= (root_ptr<std::vector<V>> const & o) const
            {
                return pi_ >= o.pi_;
            }

        friend std::ostream & operator << (std::ostream & os, root_ptr const & o) 
        {
            return os << o.operator void const * ();
        }

        ~root_ptr()
        {
            //if (po_ && ! cyclic() && header()->use_count() == 1)
            //    BOOST_LOG_TRIVIAL(info) << "\"" << pn_ << "\":" << boost::stacktrace::stacktrace(1, 1);
        }
    };


template <typename T>
    class root_ptr<std::vector<T>> : public node_ptr<std::vector<T>>
    {
        template <typename> friend class root_ptr;
        
        typedef node_ptr<std::vector<T>> base;
        
    protected:
        using base::po_;
        using base::header;
        using base::cyclic;

        /** Iterator. */
        T * pi_;
        char const * const pn_;

    public:
        typedef typename base::value_type value_type;

        using base::proxy;
        using base::reset;


        root_ptr(root_ptr const & p)
        : base(p)
        , pi_(p.pi_)
        , pn_(p.pn_)
        {
        }

        /** Allowed for C only. */
                
        root_ptr(root_ptr<std::vector<void>> const & p)
        : base(p)
        , pi_(static_cast<T *>(p.pi_))
        , pn_(p.pn_)
        {
        }
        
        /** Disabling random convertions. */
                
        template <typename V>
            root_ptr(root_ptr<std::vector<V>> const & p) = delete;
        
        root_ptr(node_proxy const & x, char const * n) 
        : base(x)
        , pi_(nullptr)
        , pn_(n)
        {
        }

        root_ptr(node_proxy const & x, char const * n, T const * p)
        : base(x)
        , pi_(const_cast<T *>(p))
        , pn_(n)
        {
        }

        template <size_t S>
            root_ptr(node_proxy const & x, char const * n, T const (& a)[S])
            : base(x)
            , pi_(const_cast<T *>(a))
            , pn_(n)
            {
            }

        /** Allowed for C only. */
            
        root_ptr(node_proxy const & x, char const * n, root_ptr<std::vector<void>> const & p)
        : base(p)
        , pi_(static_cast<T *>(p.pi_))
        , pn_(n)
        {
        }

        template <typename PoolAllocator>
            root_ptr(node_proxy const & x, char const * n, node<std::vector<T>, PoolAllocator> * p) 
            : base(x, p)
            , pi_(p->element()->data())
            , pn_(n)
            {
            }

        template <typename V, typename PoolAllocator>
            root_ptr(node_proxy const & x, char const * n, node<V, PoolAllocator> * p) = delete;

            root_ptr(node_proxy const & x, char const * n, root_ptr const & p) 
            : base(p)
            , pi_(p.pi_)
            , pn_(n)
            {
            }
            
        char const * const name() const
        {
            return pn_;
        }

        root_ptr & operator = (T const * p)
        {
            pi_ = const_cast<T *>(p);
            
            return * this;
        }
        
        template <typename V>
            root_ptr & operator = (V const * p)
            {
                pi_ = static_cast<T *>(const_cast<V *>(p));
                
                return * this;
            }

        template <typename PoolAllocator>
            root_ptr & operator = (node<std::vector<T>, PoolAllocator> * p)
            {
                pi_ = p->element()->data();
                
                return static_cast<root_ptr &>(base::operator = (p));
            }

            root_ptr & operator = (root_ptr const & p)
            {
                pi_ = p.pi_;
                
                return static_cast<root_ptr &>(base::operator = (p));
            }

        template <typename V, typename PoolAllocator>
            root_ptr & operator = (node<V, PoolAllocator> * p) = delete;

        template <typename V>
            T & operator [] (V n)
            {
                return * (pi_ + n); 
            }

        template <typename V>
            T const & operator [] (V n) const
            {
                return * (pi_ + n); 
            }

        T & operator * () const
        {
            if (po_)
                if (pi_ < po_->data() || po_->data() + po_->size() <= pi_)
                    throw std::out_of_range(std::string("\"") + name() + "\" out of range");
                
            return * pi_;
        }

        T * operator -> () const
        {
            if (po_)
                if (pi_ < po_->data() || po_->data() + po_->size() <= pi_)
                    throw std::out_of_range(std::string("\"") + name() + "\" out of range");
                
            return pi_;
        }
        
        operator bool () const
        {
            return pi_ != 0;
        }

        bool operator ! () const
        {
            return pi_ == 0;
        }

        operator T * ()
        {
            return pi_;
        }

        operator T const * () const
        {
            return pi_;
        }
        
        root_ptr & operator ++ ()
        {
            return ++ pi_, * this;
        }

        root_ptr & operator -- ()
        {
            return -- pi_, * this;
        }
        
        root_ptr operator ++ (int)
        {
            root_ptr temp(* this);
            
            return ++ pi_, temp;
        }

        root_ptr operator -- (int)
        {
            root_ptr temp(* this);
            
            return -- pi_, temp;
        }
        
        size_t operator + (root_ptr const & o) const
        {
            return pi_ + o.pi_;
        }

        size_t operator - (root_ptr const & o) const
        {
            return pi_ - o.pi_;
        }
        
        template <typename V>
            root_ptr operator + (V i) const
            {
                return root_ptr(proxy(), "", pi_ + i);
            }

        template <typename V>
            root_ptr operator - (V i) const
            {
                return root_ptr(proxy(), "", pi_ - i);
            }

        template <typename V>
            root_ptr & operator += (V i)
            {
                pi_ += i;
                
                return * this;
            }

        template <typename V>
            root_ptr & operator -= (V i)
            {
                pi_ -= i;
                
                return * this;
            }
        
        template <typename V>
            bool operator == (root_ptr<std::vector<V>> const & o) const
            {
                return pi_ == o.pi_;
            }

        template <typename V>
            bool operator != (root_ptr<std::vector<V>> const & o) const
            {
                return pi_ != o.pi_;
            }

        template <typename V>
            bool operator < (root_ptr<std::vector<V>> const & o) const
            {
                return pi_ < o.pi_;
            }

        template <typename V>
            bool operator > (root_ptr<std::vector<V>> const & o) const
            {
                return pi_ > o.pi_;
            }

        template <typename V>
            bool operator <= (root_ptr<std::vector<V>> const & o) const
            {
                return pi_ <= o.pi_;
            }

        template <typename V>
            bool operator >= (root_ptr<std::vector<V>> const & o) const
            {
                return pi_ >= o.pi_;
            }

        friend std::ostream & operator << (std::ostream & os, root_ptr const & o) 
        {
            return os << o.operator T const * ();
        }
        
        ~root_ptr()
        {
            //if (po_ && ! cyclic() && header()->use_count() == 1)
            //    BOOST_LOG_TRIVIAL(info) << "\"" << pn_ << "\":" << boost::stacktrace::stacktrace(1, 1);
        }
    };


BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION(__construct)


template <typename T, bool = has_static_member_function___construct<T, T (node_proxy const &, char const *, T const &)>::value>
    struct construct
    {
        template <typename... Args>
            inline T operator () (node_proxy const & __y, char const * n, Args &&... args)
            {
                return T(std::forward<Args>(args)...);
            }
    };

template <typename T, size_t S>
    struct construct<root_array<T, S>, false>
    {
        template <typename... Args>
            inline root_array<T, S> operator () (node_proxy const & __y, char const * n, Args &&... args)
            {
                return root_array<T, S>(__y, std::forward<Args>(args)...);
            }
    };

template <typename T>
    struct construct<root_ptr<T>, false>
    {
        template <typename... Args>
            inline root_ptr<T> operator () (node_proxy const & __y, char const * n, Args &&... args)
            {
                return root_ptr<T>(__y, n, std::forward<Args>(args)...);
            }
    };

template <typename T>
    struct construct<T, true>
    {
        template <typename... Args>
            inline T operator () (node_proxy const & __y, char const * n, Args &&... args)
            {
                return T::__construct(__y, n, std::forward<Args>(args)...);
            }
    };
    
template <typename T>
    inline T make_construct(node_proxy const & __y, char const * n, T const & po)
    {
        return construct<T>()(__y, n, po);
    }
    
    
template <typename T>
    struct create
    {
        template <typename... Args>
            inline node<std::vector<T>> * operator () (node_proxy const & __y, size_t s, Args &&... args)
            {
                return new node<std::vector<T>>(s, construct<T>()(__y, "", std::forward<Args>(args)...));
            }
            
            inline node<std::vector<T>> * operator () (node_proxy const & __y, std::initializer_list<T> const & l)
            {
                return new node<std::vector<T>>(l);
            }
    };


template <typename T, size_t S>
    class root_array : public boost::root_ptr<T>
    {
        typedef boost::root_ptr<T> base;
        
    public:
        root_array(base const & p) : base(p)
        {
        }
        
        root_array(boost::node_proxy const & __y) : base(__y, "", boost::create<typename T::value_type>()(__y, S))
        {
        }
    
        template <typename... U>
            root_array(boost::node_proxy const & __y, U const &... pp) : base(__y, "", boost::create<typename T::value_type>()(__y, std::initializer_list<typename T::value_type>{static_cast<typename T::value_type>(pp)...}))
            {
            }
            
        ~root_array()
        {
            base::reset(nullptr);
        }
    };    


/** The returned size is in bytes! */
        
template <typename T>
    struct size_of_t
    {
        static size_t const value = sizeof(T);
    };

template <typename T, size_t S>
    struct size_of_t<root_array<T, S>>
    {
        static size_t const value = sizeof(T) * S;
    };

template <typename T>
    inline size_t size_of(T const &)
    {
        return sizeof(T);
    }

template <typename T, size_t S>
    inline size_t size_of(root_array<T, S> const &)
    {
        return sizeof(T) * S;
    }


/**
    Instanciates a new @c node_ptr<> .
*/

template <typename V, typename... Args, typename PoolAllocator = pool_allocator<V> >
    inline node_ptr<V> make_node(Args &&... args)
    {
        return node_ptr<V>(new node<V, PoolAllocator>(std::forward<Args>(args)...));
    }


/**
    Static cast.
*/

template <typename T, typename V>
    inline node_ptr<T> static_pointer_cast(node_ptr<V> const & p)
    {
        return node_ptr<T>(p, smart_ptr::detail::static_cast_tag());
    }


/**
    Dynamic cast.
*/

template <typename T, typename V>
    inline node_ptr<T> dynamic_pointer_cast(node_ptr<V> const & p)
    {
        return node_ptr<T>(p, smart_ptr::detail::dynamic_cast_tag());
    }


/**
    Comparison operator.

    @param  a1  Operand 1.
    @param  a2  Operand 2.
*/

template <typename T>
    inline bool operator == (node_ptr<T> const &a1, node_ptr<T> const &a2)
    {
        return a1.get() == a2.get();
    }


/**
    Comparison operator.

    @param  a1  Operand 1.
    @param  a2  Operand 2.
*/

template <typename T>
    inline bool operator != (node_ptr<T> const &a1, node_ptr<T> const &a2)
    {
        return a1.get() != a2.get();
    }


} // namespace boost


#if defined(_MSC_VER)
#pragma warning( pop )
#endif


#endif // #ifndef BOOST_NODE_PTR_INCLUDED
