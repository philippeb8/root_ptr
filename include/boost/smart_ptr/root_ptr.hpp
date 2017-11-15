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

#include <utility>

#ifndef BOOST_DISABLE_THREADS
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#endif

//#include <boost/stacktrace.hpp>
//#include <boost/log/trivial.hpp>
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
    template <typename> friend class node_ptr;

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
        Disabled copy constructor.
    */

    node_proxy(node_proxy const & x) = delete;


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
            //BOOST_LOG_TRIVIAL(info) << "<cycle>:" << boost::stacktrace::stacktrace(2, 1);
            
            ++ n;
            delete &* m;
        }

        destroying(false);
    }
};


template <typename T>
    inline void proxy(node_proxy const & __y, T const & po)
    {
    }


template <typename T, size_t S>
    inline void proxy(node_proxy const & __y, T const (& po)[S])
    {
        for (T const * i = po; i != po + S; ++ i)
            proxy(__y, * i);
    }


template <typename T>
    inline void proxy(node_proxy const & __y, std::vector<T> const & po)
    {
        for (typename std::vector<T>::const_iterator i = po.begin(); i != po.end(); ++ i)
            proxy(__y, * i);
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
                
                proxy(* p.px_);
                
                base::operator = (p);

                return * this;
            }
            
            
        /**
            Returns associated proxy.
            
            @return     @c node_proxy part of @c node_ptr .
        */
        
        node_proxy const & proxy() 
        {
            return *px_;
        }

        
        /**
            Returns associated proxy.
            
            @return     @c node_proxy part of @c node_ptr .
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
            if (x.depth() < px_->depth())
            {
                px_ = & x;
                
                if (po_)
                {
                    header()->node_tag_.erase();
                    px_->init(header());
                    boost::proxy(* px_, * po_);
                }
            }
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
    class root_ptr : public node_ptr<T>
    {
        typedef node_ptr<T> base;
        typedef typename base::value_type value_type;
        
    protected:
        using base::po_;
        using base::header;
        using base::cyclic;
        
        char const * const pn_;

    public:
        root_ptr(node_proxy const & x, char const * n) 
        : base(x)
        , pn_(n)
        {
        }

        template <typename V, typename PoolAllocator>
            root_ptr(node_proxy const & x, char const * n, node<V, PoolAllocator> * p) 
            : base(x, p)
            , pn_(n)
            {
            }

            root_ptr(char const * n, root_ptr<value_type> const & p) 
            : base(p)
            , pn_(n)
            {
            }

        template <typename V, typename PoolAllocator>
            root_ptr & operator = (node<V, PoolAllocator> * p)
            {
                return static_cast<root_ptr &>(base::operator = (p));
            }

        template <typename V>
            root_ptr & operator = (root_ptr<V> const & p)
            {
                return static_cast<root_ptr &>(base::operator = (p));
            }

            root_ptr & operator = (root_ptr<value_type> const & p)
            {
                return static_cast<root_ptr &>(base::operator = (p));
            }

        value_type & operator * () const
        {
            return * po_;
        }

        value_type * operator -> () const
        {
            return po_;
        }
        
        operator value_type * () const
        {
            return po_;
        }
        
        friend std::ostream & operator << (std::ostream & os, const root_ptr & o) 
        {
            return os << o.operator value_type * ();
        }
        
        ~root_ptr()
        {
            //if (po_ && header()->use_count() == 1 && ! cyclic())
            //    BOOST_LOG_TRIVIAL(info) << "\"" << pn_ << "\":" << boost::stacktrace::stacktrace(1, 1);
        }
    };


#if !defined(_MSC_VER)
template <typename T, std::size_t N>
    class root_ptr<T [N]> : public node_ptr<T [N]>
    {
        typedef node_ptr<T [N]> base;
        
    protected:
        using base::po_;
        using base::header;
        using base::cyclic;

        /** Iterator. */
        T * pi_;
        char const * const pn_;

    public:
        typedef typename base::value_type value_type;


        root_ptr(node_proxy const & x, char const * n) 
        : base(x)
        , pi_(nullptr)
        , pn_(n)
        {
        }

        root_ptr(node_proxy const & x, char const * n, T * p) 
        : base(x)
        , pi_(p)
        , pn_(n)
        {
        }

        template <typename PoolAllocator>
            root_ptr(node_proxy const & x, char const * n, node<T [N], PoolAllocator> * p) 
            : base(x, p)
            , pi_(p->element())
            , pn_(n)
            {
            }

            root_ptr(char const * n, root_ptr<value_type> const & p) 
            : base(p)
            , pi_(p.pi_)
            , pn_(n)
            {
            }

        template <typename PoolAllocator>
            root_ptr & operator = (node<T [N], PoolAllocator> * p)
            {
                pi_ = p->element()->data();
                
                return static_cast<root_ptr &>(base::operator = (p));
            }

            root_ptr & operator = (root_ptr<value_type> const & p)
            {
                pi_ = p.pi_;
                
                return static_cast<root_ptr &>(base::operator = (p));
            }

        T & operator [] (std::size_t n)
        {
            if (pi_ - po_ + n < N)
                return * (pi_ + n); 
            else 
                throw std::out_of_range("assertion failed: " + std::to_string(pi_ - po_) + " < " + std::to_string(N));
        }

        T const & operator [] (std::size_t n) const
        {
            if (pi_ - po_ + n < N)
                return * (pi_ + n); 
            else 
                throw std::out_of_range("assertion failed: " + std::to_string(pi_ - po_) + " < " + std::to_string(N));
        }

        T & operator * () const
        {
            if (pi_)
                return * pi_;
            else
                throw std::out_of_range("null pointer");
        }

        T * operator -> () const
        {
            if (pi_)
                return pi_;
            else
                throw std::out_of_range("null pointer");
        }
        
        operator bool () const
        {
            return pi_ != 0;
        }

        bool operator ! () const
        {
            return pi_ == 0;
        }

        operator T * () const
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
        
        friend std::ostream & operator << (std::ostream & os, const root_ptr & o) 
        {
            return os << o.operator T * ();
        }
        
        ~root_ptr()
        {
            //if (po_ && header()->use_count() == 1 && ! cyclic())
            //    BOOST_LOG_TRIVIAL(info) << "\"" << pn_ << "\":" << boost::stacktrace::stacktrace(1, 1);
        }
    };
#endif


template <typename T>
    class root_ptr<std::vector<T>> : public node_ptr<std::vector<T>>
    {
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


        root_ptr(node_proxy const & x, char const * n) 
        : base(x)
        , pi_(nullptr)
        , pn_(n)
        {
        }

        root_ptr(node_proxy const & x, char const * n, T * p) 
        : base(x)
        , pi_(p)
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

            root_ptr(char const * n, root_ptr<value_type> const & p) 
            : base(p)
            , pi_(p.pi_)
            , pn_(n)
            {
            }

        root_ptr & operator = (T * p)
        {
            pi_ = p;
            
            return * this;
        }

        template <typename PoolAllocator>
            root_ptr & operator = (node<std::vector<T>, PoolAllocator> * p)
            {
                pi_ = p->element()->data();
                
                return static_cast<root_ptr &>(base::operator = (p));
            }

            root_ptr & operator = (root_ptr<value_type> const & p)
            {
                pi_ = p.pi_;
                
                return static_cast<root_ptr &>(base::operator = (p));
            }

        T & operator [] (std::size_t n)
        {
            if (pi_ - po_->data() + n < po_->size()) 
                return * (pi_ + n); 
            else 
                throw std::out_of_range("assertion failed: " + std::to_string(pi_ - po_->data()) + " < " + std::to_string(po_->size()));
        }

        T const & operator [] (std::size_t n) const
        {
            if (pi_ - po_->data() + n < po_->size()) 
                return * (pi_ + n); 
            else 
                throw std::out_of_range("assertion failed: " + std::to_string(pi_ - po_->data()) + " < " + std::to_string(po_->size()));
        }

        T & operator * () const
        {
            if (pi_)
                return * pi_;
            else
                throw std::out_of_range("null pointer");
        }

        T * operator -> () const
        {
            if (pi_)
                return pi_;
            else
                throw std::out_of_range("null pointer");
        }
        
        operator bool () const
        {
            return pi_ != 0;
        }

        bool operator ! () const
        {
            return pi_ == 0;
        }

        operator T * () const
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

        friend std::ostream & operator << (std::ostream & os, const root_ptr & o) 
        {
            return os << o.operator T * ();
        }

        template <typename... Args>
            void emplace_resize(size_t s, Args const &... args)
            {
                if (s < po_->size())
                    po_->resize(s);
                else if (po_->size() < s)
                {
                    po_->reserve(s);
                    
                    for (size_t i = po_->size(); i < s; ++ i)
                        po_->emplace_back(args...);
                    
                    pi_ = po_->data();
                }
            }
        
        ~root_ptr()
        {
            //if (po_ && header()->use_count() == 1 && ! cyclic())
            //    BOOST_LOG_TRIVIAL(info) << "\"" << pn_ << "\":" << boost::stacktrace::stacktrace(1, 1);
        }
    };


/**
    Instanciates a new @c node_ptr<> .
*/

template <typename V, typename... Args, typename PoolAllocator = pool_allocator<V> >
    inline node_ptr<V> make_node(Args const &... args)
    {
        return node_ptr<V>(new node<V, PoolAllocator>(args...));
    }


/**
    Instanciates a new @c node_ptr<> .

    @note Uses @c fast_pool_allocator to instanciate the pointee object.
*/

template <typename V, typename... Args, typename PoolAllocator = pool_allocator<V> >
    inline node_ptr<V> make_fastnode(Args const &... args)
    {
        return node_ptr<V>(new fastnode<V>(args...));
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
