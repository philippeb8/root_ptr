/*!
    \file
    \brief Boost root_ptr.hpp mainheader file.

    Patent Pending
    'SOURCE TO SOURCE COMPILER, COMPILATION METHOD, AND
    COMPUTER-READABLE MEDIUM FOR PREDICTABLE MEMORY MANAGEMENT'

    Copyright (C) 2021 Fornux Inc.

    Phil Bouchard, Founder & CEO
    Fornux Inc.
    phil@fornux.com
    20 Poirier St, Gatineau, Quebec, Canada, J8V 1A6

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

#include <cstdint>

#include <array>
#include <vector>
#include <utility>
#include <sstream>
#include <initializer_list>

#ifndef BOOST_DISABLE_THREADS
#include <boost/thread/thread.hpp>
#include <boost/thread/recursive_mutex.hpp>
#endif

#include <iostream>
#include <boost/log/trivial.hpp>
#include <boost/smart_ptr/detail/intrusive_list.hpp>
#include <boost/smart_ptr/detail/intrusive_stack.hpp>
#include <boost/smart_ptr/detail/node_base.hpp>


namespace boost
{


struct node_base;

    
#ifndef BOOST_DISABLE_THREADS
/** Main global mutex used for thread safety */
static recursive_mutex & static_mutex()
{
    static recursive_mutex mutex_;
    
    return mutex_;
}
#endif


/**
    Set header.

    Proxy object used to link a list of @c node<> blocks and a list of @c node_proxy .
*/

class node_proxy
{
    template <typename> friend class root_proxy;
    template <typename> friend class root_ptr;

    /** Filename. */
    char const * file_;

    /** Function. */
    char const * function_;
    
    /** Line. */
    unsigned line_;
    
    /** Parent. */
    node_proxy * parent_;
    
    /** Stack depth. */
    size_t const depth_;
    
    /** Destruction sequence flag. */
    bool destroying_;

    /** List of all pointee objects belonging to a @c node_proxy . */
    mutable smart_ptr::detail::intrusive_list node_list_;


public:
    /**
        Initialization of a single @c node_proxy .
    */

    node_proxy(char const * file, char const * function, unsigned line, size_t depth = 0, node_proxy * parent = nullptr) : file_(file), function_(function), line_(line), depth_(depth), parent_(parent), destroying_(false)
    {
        * top_node_proxy() = this;
    }
    
    
    static node_proxy ** top_node_proxy()
    {
        static thread_local node_proxy * p;
        
        return & p;
    }
    
    
    static std::ostream & stacktrace(std::ostream & out, node_proxy * p)
    {
        for (size_t depth = 0; p && p->depth_; ++ depth, p = p->parent_)
            out << '#' << depth << ' ' << p->function_ << " in " << p->file_ << " line " << p->line_<< '\n';
        
        return out;
    }


    /**
        Disabled copy constructor.
    */

    node_proxy(node_proxy & x) = delete;


    /**
        Function-style access.
    */
    
    node_proxy & operator () ()
    {
        return * this;
    }
    
    
    /**
        Destruction of a single @c node_proxy and detaching itself from other @c node_proxy .
    */

    ~node_proxy()
    {
        reset();

        * top_node_proxy() = parent();
    }
    
    
    node_proxy * parent() const
    {
        return parent_;
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

    void init(node_base * p) const
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


class stack_node_proxy : public node_proxy
{
public:
    /**
        Initialization of a single @c stack_node_proxy .
    */

    stack_node_proxy(char const * file, char const * function, unsigned line, node_proxy & __y) : node_proxy(file, function, line, __y.depth() + 1, & __y)
    {
    }
    
    
    /**
        Destruction of a single @c stack_node_proxy .
     */
    
    ~stack_node_proxy()
    {
    }
};


struct static_cast_tag {};
struct dynamic_cast_tag {};


/**
    Smart pointer optimized for speed and memory usage.
    
    This class represents a basic smart pointer interface.
*/


class root_core
{
    typedef node_base value_type;

protected:
    value_type * po_;

public:
    explicit root_core() 
    : po_(nullptr)
    {
    }

    ~root_core()
    {
#ifndef BOOST_DISABLE_THREADS
        recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
            
        if (po_)
        {
            po_->release();
        }
    }

    template <typename V, typename PoolAllocator>
        explicit root_core(node<V, PoolAllocator> * p) 
        : po_(p)
        {
        }

#if 0 //defined(BOOST_HAS_RVALUE_REFS)
    root_core(root_core && p)
    : po_(std::move(p.po_))
    {
    }
#endif

        root_core(root_core const & p) 
        : po_(p.share())
        {
        }

    template <typename V, typename PoolAllocator>
        root_core & operator = (node<V, PoolAllocator> * p)
        {
            reset(p);
            
            return * this;
        }
    
    root_core & operator = (root_core const & p)
    {
        if (po_ != p.po_)
        {
            reset(p.share());
        }
        
        return * this;
    }

    value_type * get() const
    {
        return po_;
    }

    value_type * share() const
    {
        if (po_)
        {
            po_->add_ref_copy();
        }
        
        return po_;
    }

    void reset(value_type * p)
    {
        if (po_)
        {
            po_->release();
        }
        
        po_ = p;
    }

    long use_count() const // never throws
    {
        return po_->use_count();
    }
};


/**
    Deterministic region based memory manager.

    Complete memory management utility on top of standard reference counting.

    @note Must be initialized with a reference to a @c node_proxy , given by a @c root_proxy<> .
*/

template <typename T>
class root_proxy : public root_core
    {
        template <typename> friend class root_proxy;

        
    protected:
        /** Reference to the @c node_proxy node @c root_proxy<> belongs to. */
        mutable node_proxy * px_;
        

    public:
        typedef root_core base;
        
        
        /**
            Initialization of a pointer.

            @param  x   Reference to a @c node_proxy the pointer belongs to.
        */

        explicit root_proxy(node_proxy & x)
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
            explicit root_proxy(node_proxy & x, node<V, PoolAllocator> * p)
            : base(p)
            , px_(& x)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
                
                px_->init(p);
            }


    public:
        typedef T value_type;


        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            root_proxy(root_proxy<V> const & p)
            : base(p)
            , px_(p.px_)
            {
            }

            
#if 0 //defined(BOOST_HAS_RVALUE_REFS)
        template <typename V>
            root_proxy(root_proxy<V> && p)
            : base(std::move(p))
            , px_(std::move(p.px_))
            {
            }
#endif


        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

            root_proxy(root_proxy<T> const & p)
            : base(p)
            , px_(p.px_)
            {
            }

            
        /**
            Assignment.

            @param  p   New pointer expected to be null.
        */

        root_proxy & operator = (std::nullptr_t)
        {
            base::reset(nullptr);
            
            return * this;
        }


        /**
            Assignment.

            @param  p   New pointer to manage.
        */

        template <typename V, typename PoolAllocator>
            root_proxy & operator = (node<V, PoolAllocator> * p)
            {
                px_->init(p);

                base::operator = (p);
                
                return * this;
            }

            
        /**
            Assignment.

            @param  p New pointer to manage.
        */

        template <typename V>
            root_proxy & operator = (root_proxy<V> const & p)
            {
                if (p.px_->depth() < px_->depth())
                    proxy(p.px_);
                else if (px_->depth() < p.px_->depth())
                    p.proxy(px_);
                
                base::operator = (static_cast<typename root_proxy<V>::base const &>(p));
                
                return * this;
            }


        /**
            Assignment.

            @param  p New pointer to manage.
        */

            root_proxy & operator = (root_proxy const & p)
            {
                if (p.px_->depth() < px_->depth())
                    proxy(p.px_);
                else if (px_->depth() < p.px_->depth())
                    p.proxy(px_);
                
                base::operator = (static_cast<root_proxy::base const &>(p));
                
                return * this;
            }

            
        /**
            Assignment.

            @param  p   New pointer to manage.
        */

        template <typename V>
            void reset(root_proxy<V> const & p)
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
            
            @return     @c node_proxy part of @c root_proxy .
        */
        
        node_proxy & proxy() 
        {
            return * px_;
        }

        
        /**
            Returns associated proxy.
            
            @return     @c node_proxy part of @c root_proxy .
        */
        
        node_proxy & proxy() const
        {
            return * px_;
        }

        
        /**
            Sets associated proxy for the entire branch of this pointer.
        */
        
        void proxy(node_proxy * x) const
        {
            if (x->depth() < px_->depth())
            {
                px_ = x;

                if (base::get())
                {
                    base::get()->node_tag_.erase();
                    x->init(base::get());
                    base::get()->proxy(x);
                }
            }
        }


        /**
            Explicit cyclicism detection mechanism, for use @b only inside destructors.

            @return Whether the pointer inside a destructor points to an object already destroyed.

        */

        bool cyclic() const
        {
            return px_->destroying();
        }
        

        /**
            Destructor.
        */

        ~root_proxy()
        {
            if (cyclic())
            {
                base::po_ = nullptr;
            }
        }
    };


template <>
    class root_ptr<std::nullptr_t> : public root_proxy<std::nullptr_t>
    {
        template <typename> friend class root_ptr;
        
        template <typename U, typename V> friend U static_pointer_cast(V const & p);
        template <typename U, typename V> friend U dynamic_pointer_cast(V const & p);
        template <typename U, typename V> friend U reinterpret_pointer_cast(V const & p);
        template <typename V> friend root_ptr<V const> const_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V> const_pointer_cast(root_ptr<V const> const & p);
        
        typedef root_proxy<std::nullptr_t> base;
        
    protected:
        /** Iterator. */
        std::nullptr_t * pi_;
        char const * pn_;

    public:
        typedef typename base::value_type value_type;


        root_ptr(node_proxy & x, char const * n, std::nullptr_t p)
        : base(x)
        , pi_(p)
        , pn_(n)
        {
        }

        char const * name() const
        {
            return pn_;
        }

        operator bool () const
        {
            return pi_ != 0;
        }

        bool operator ! () const
        {
            return pi_ == 0;
        }
        
        operator std::nullptr_t * () const
        {
            return pi_;
        }
        
        template <typename V>
            bool operator == (root_ptr<V> const & o) const
            {
                return pi_ == o.pi_;
            }

        template <typename V>
            bool operator != (root_ptr<V> const & o) const
            {
                return pi_ != o.pi_;
            }

        friend std::ostream & operator << (std::ostream & os, root_ptr const & o) 
        {
#ifndef BOOST_DISABLE_THREADS
            recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
                
            return os << o.operator std::nullptr_t * ();
        }

        ~root_ptr()
        {
            //if (po_ && ! cyclic() && get()->use_count() == 1)
            //    BOOSstd::nullptr_t_LOG_std::nullptr_tRIVIAL(info) << "\"" << pn_ << "\":" << boost::stacktrace::stacktrace(1, 1);
        }
    };
    
    
template <typename T>
    class root_ptr : public root_proxy<T>
    {
        template <typename> friend class root_ptr;
        
        template <typename U, typename V> friend U static_pointer_cast(V const & p);
        template <typename U, typename V> friend U dynamic_pointer_cast(V const & p);
        template <typename U, typename V> friend U reinterpret_pointer_cast(V const & p);
        template <typename V> friend root_ptr<V const> const_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V> const_pointer_cast(root_ptr<V const> const & p);
        
        typedef root_proxy<T> base;
        
    protected:
        /** Iterator. */
        T * pi_;
        char const * pn_;

    public:
        typedef typename base::value_type value_type;


        root_ptr(root_ptr const & p)
        : base(p)
        , pi_(p.pi_)
        , pn_(p.pn_)
        {
        }

        template <typename V>
            explicit root_ptr(root_ptr<V> const & p)
            : base(p)
            , pi_(static_cast<T *>(p.pi_))
            , pn_(p.pn_)
            {
            }

            root_ptr(root_ptr<std::nullptr_t> const & p)
            : base(p.proxy())
            , pi_(nullptr)
            , pn_(p.pn_)
            {
            }
            
#if 0 //defined(BOOST_HAS_RVALUE_REFS)
        template <typename V>
            root_ptr(root_ptr<V> && p)
            : base(std::move(p))
            , pi_(std::move(p.pi_))
            , pn_(std::move(p.pn_))
            {
            }
#endif

        root_ptr(node_proxy & x, char const * n) 
        : base(x)
        , pi_(nullptr)
        , pn_(n)
        {
        }

        root_ptr(node_proxy & x, char const * n, std::nullptr_t p)
        : base(x)
        , pi_(p)
        , pn_(n)
        {
        }
        
        explicit root_ptr(node_proxy & x, char const * n, std::uintptr_t p)
        : base(x)
        , pi_(reinterpret_cast<T *>(p))
        , pn_(n)
        {
        }
        
        template <typename V>
            explicit root_ptr(node_proxy & x, char const * n, V * p)
            : base(x)
            , pi_(static_cast<T *>(p))
            , pn_(n)
            {
            }

        template <typename V>
            explicit root_ptr(node_proxy & x, char const * n, V const * p)
            : base(x)
            , pi_(static_cast<T *>(const_cast<V *>(p)))
            , pn_(n)
            {
            }
        
        template <typename V, typename PoolAllocator>
            root_ptr(node_proxy & x, char const * n, node<V, PoolAllocator> * p) 
            : base(x, p)
            , pi_(static_cast<T *>(p->data()))
            , pn_(n)
            {
            }

            root_ptr(node_proxy & x, char const * n, root_ptr const & p) 
            : base(p)
            , pi_(p.pi_)
            , pn_(n)
            {
            }
            
        template <typename V>
            explicit root_ptr(node_proxy & x, char const * n, root_ptr<V> const & p) 
            : base(p)
            , pi_(static_cast<T *>(p.pi_))
            , pn_(n)
            {
            }
        
            
        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            root_ptr(root_ptr<V> const & p, char const * n, static_cast_tag const & t)
            : base(p, t)
            , pi_(static_cast<T *>(p.pi_))
            , pn_(n)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif

                if (! pi_)
                {
                    std::stringstream out;
                    out << "\"" << name() << "\" internal error\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }            
            }

        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            root_ptr(root_ptr<V> const & p, char const * n, dynamic_cast_tag const & t)
            : base(p, t)
            , pi_(dynamic_cast<T *>(p.pi_))
            , pn_(n)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
                
                if (! pi_)
                {
                    std::stringstream out;
                    out << "\"" << name() << "\" internal error\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }            
            }

        char const * name() const
        {
            return pn_;
        }

        root_ptr & operator = (std::nullptr_t)
        {
#ifndef BOOST_DISABLE_THREADS
            recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif        

            pi_ = nullptr;
            
            return static_cast<root_ptr &>(base::operator = (nullptr));
        }
        
        template <typename V>
            root_ptr & operator = (V const * p)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif        

                pi_ = const_cast<V *>(p);
                
                return * this;
            }

        template <typename V, typename PoolAllocator>
            root_ptr & operator = (node<V, PoolAllocator> * p)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif        

                pi_ = static_cast<V *>(p->data());
                
                return static_cast<root_ptr &>(base::template operator = <V, PoolAllocator>(p));
            }

        template <typename V>
            root_ptr & operator = (root_ptr<V> const & p)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif        

                pi_ = p.pi_;
                
                return static_cast<root_ptr &>(base::template operator = <V>(p));
            }

            root_ptr & operator = (root_ptr const & p)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif        

                pi_ = p.pi_;
                
                return static_cast<root_ptr &>(base::operator = (p));
            }

        template <typename V>
            T & operator [] (V n)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
                
                if (! pi_)
                {
                    std::stringstream out;
                    out << "\"" << name() << "\" is a null pointer\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }            
                
                if (base::base::get() && base::base::get()->size() <= n)
                {
                    std::stringstream out;
                    out << "\"" << name() << "\" (" << n << ") is out of range [0, " << base::base::get()->size() << "[\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }

                return * (pi_ + n); 
            }

        template <typename V>
            T const & operator [] (V n) const
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
            
                if (! pi_)
                {
                    std::stringstream out;
                    out << "\"" << name() << "\" is a null pointer\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }            
                
                if (base::base::get() && base::base::get()->size() <= n)
                {
                    std::stringstream out;
                    out << "\"" << name() << "\" (" << n << ") is out of range [0, " << base::base::get()->size() << "[\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }
                
                return * (pi_ + n); 
            }

        T & operator * () const
        {
#ifndef BOOST_DISABLE_THREADS
            recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
            
            if (! pi_)
            {
                std::stringstream out;
                out << "\"" << name() << "\" is a null pointer\n";
                node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                throw std::out_of_range(out.str());
            }         
            
            if (base::base::get() && (base::base::get()->size() == 0 || pi_ < static_cast<T *>(base::base::get()->data()) || pi_ >= static_cast<T *>(base::base::get()->data()) + base::base::get()->size()))
            {
                std::stringstream out;
                out << "\"" << name() << "\" (" << pi_ - static_cast<T *>(base::base::get()->data()) << ") is out of range [0, " << base::base::get()->size() << "[\n";
                node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                throw std::out_of_range(out.str());
            }
                
            return * pi_;
        }

        T * operator -> () const
        {
#ifndef BOOST_DISABLE_THREADS
            recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
            
            if (! pi_)
            {
                std::stringstream out;
                out << "\"" << name() << "\" is a null pointer\n";
                node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                throw std::out_of_range(out.str());
            }            
            
            if (base::base::get() && (base::base::get()->size() == 0 || pi_ < static_cast<T *>(base::base::get()->data()) || pi_ >= static_cast<T *>(base::base::get()->data()) + base::base::get()->size()))
            {
                std::stringstream out;
                out << "\"" << name() << "\" (" << pi_ - static_cast<T *>(base::base::get()->data()) << ") is out of range [0, " << base::base::get()->size() << "[\n";
                node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                throw std::out_of_range(out.str());
            }

            return pi_;
        }
        
        root_ptr<root_ptr<T>> operator & () const
        {
            return root_ptr<root_ptr<T>>(base::proxy(), "", this);
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
            if (! pi_)
            {
                std::stringstream out;
                out << "\"" << name() << "\" is a null pointer\n";
                node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                throw std::out_of_range(out.str());
            }            
            
            if (base::base::get() && (base::base::get()->size() == 0 || pi_ < static_cast<T *>(base::base::get()->data()) || pi_ >= static_cast<T *>(base::base::get()->data()) + base::base::get()->size()))
            {
                std::stringstream out;
                out << "\"" << name() << "\" (" << pi_ - static_cast<T *>(base::base::get()->data()) << ") is out of range [0, " << base::base::get()->size() << "[\n";
                node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                throw std::out_of_range(out.str());
            }

            return pi_;
        }

        root_ptr & operator ++ ()
        {
#ifndef BOOST_DISABLE_THREADS
            recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
            
            return ++ pi_, * this;
        }

        root_ptr & operator -- ()
        {
#ifndef BOOST_DISABLE_THREADS
            recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
            
            return -- pi_, * this;
        }
        
        root_ptr operator ++ (int)
        {
#ifndef BOOST_DISABLE_THREADS
            recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
            
            root_ptr temp(* this);
            
            return ++ pi_, temp;
        }

        root_ptr operator -- (int)
        {
#ifndef BOOST_DISABLE_THREADS
            recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
            
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
                return root_ptr(base::proxy(), "", pi_ + i);
            }

        template <typename V>
            root_ptr operator - (V i) const
            {
                return root_ptr(base::proxy(), "", pi_ - i);
            }

        template <typename V>
            root_ptr & operator += (V i)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
                
                pi_ += i;
                
                return * this;
            }

        template <typename V>
            root_ptr & operator -= (V i)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
                
                pi_ -= i;
                
                return * this;
            }
        
        template <typename V>
            bool operator == (root_ptr<V> const & o) const
            {
                return pi_ == o.pi_;
            }

        template <typename V>
            bool operator != (root_ptr<V> const & o) const
            {
                return pi_ != o.pi_;
            }

            bool operator == (root_ptr<std::nullptr_t> const & o) const
            {
                return pi_ == nullptr;
            }

            bool operator != (root_ptr<std::nullptr_t> const & o) const
            {
                return pi_ != nullptr;
            }

        template <typename V>
            bool operator < (root_ptr<V> const & o) const
            {
                return pi_ < o.pi_;
            }

        template <typename V>
            bool operator > (root_ptr<V> const & o) const
            {
                return pi_ > o.pi_;
            }

        template <typename V>
            bool operator <= (root_ptr<V> const & o) const
            {
                return pi_ <= o.pi_;
            }

        template <typename V>
            bool operator >= (root_ptr<V> const & o) const
            {
                return pi_ >= o.pi_;
            }

        friend std::ostream & operator << (std::ostream & os, root_ptr const & o) 
        {
#ifndef BOOST_DISABLE_THREADS
            recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
                
            return os << o.operator T * ();
        }

        ~root_ptr()
        {
            //if (po_ && ! cyclic() && get()->use_count() == 1)
            //    BOOST_LOG_TRIVIAL(info) << "\"" << pn_ << "\":" << boost::stacktrace::stacktrace(1, 1);
        }
    };

    
template <>
    class root_ptr<void> : public root_proxy<void>
    {
        template <typename> friend class root_ptr;
        
        template <typename U, typename V> friend U static_pointer_cast(V const & p);
        template <typename U, typename V> friend U dynamic_pointer_cast(V const & p);
        template <typename U, typename V> friend U reinterpret_pointer_cast(V const & p);
        template <typename V> friend root_ptr<V const> const_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V> const_pointer_cast(root_ptr<V const> const & p);
        
        typedef root_proxy<void> base;
        
    protected:
        /** Iterator. */
        void * pi_;
        char const * pn_;

    public:
        typedef typename base::value_type value_type;


        root_ptr(root_ptr const & p)
        : base(p)
        , pi_(p.pi_)
        , pn_(p.pn_)
        {
        }

        template <typename V>
            root_ptr(root_ptr<V> const & p)
            : base(p)
            , pi_(p.pi_)
            , pn_(p.pn_)
            {
            }
            
            root_ptr(root_ptr<std::nullptr_t> const & p)
            : base(p.proxy())
            , pi_(nullptr)
            , pn_(p.pn_)
            {
            }
            
#if 0 //defined(BOOST_HAS_RVALUE_REFS)
        template <typename V>
            root_ptr(root_ptr<V> && p)
            : base(std::move(p))
            , pi_(std::move(p.pi_))
            , pn_(std::move(p.pn_))
            {
            }
#endif

        root_ptr(node_proxy & x, char const * n) 
        : base(x)
        , pi_(nullptr)
        , pn_(n)
        {
        }

        root_ptr(node_proxy & x, char const * n, std::nullptr_t p)
        : base(x)
        , pi_(p)
        , pn_(n)
        {
        }
        
        explicit root_ptr(node_proxy & x, char const * n, std::uintptr_t p)
        : base(x)
        , pi_(reinterpret_cast<void *>(p))
        , pn_(n)
        {
        }
        
        template <typename V>
            root_ptr(node_proxy & x, char const * n, V * p)
            : base(x)
            , pi_(p)
            , pn_(n)
            {
            }

        template <typename V>
            root_ptr(node_proxy & x, char const * n, V const * p)
            : base(x)
            , pi_(const_cast<V *>(p))
            , pn_(n)
            {
            }
        
        template <typename V, typename PoolAllocator>
            root_ptr(node_proxy & x, char const * n, node<V, PoolAllocator> * p) 
            : base(x, p)
            , pi_(static_cast<V *>(p->data()))
            , pn_(n)
            {
            }

        template <typename V>
            root_ptr(node_proxy & x, char const * n, root_ptr<V> const & p) 
            : base(p)
            , pi_(p.pi_)
            , pn_(n)
            {
            }
            
            root_ptr(node_proxy & x, char const * n, root_ptr const & p) 
            : base(p)
            , pi_(p.pi_)
            , pn_(n)
            {
            }
            
        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            root_ptr(root_ptr<V> const & p, char const * n, static_cast_tag const & t)
            : base(p, t)
            , pi_(static_cast<void *>(p.pi_))
            , pn_(n)
            {
            }


        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            root_ptr(root_ptr<V> const & p, char const * n, dynamic_cast_tag const & t)
            : base(p, t)
            , pi_(dynamic_cast<void *>(p.pi_))
            , pn_(n)
            {
            }


        char const * name() const
        {
            return pn_;
        }

        root_ptr & operator = (std::nullptr_t)
        {
#ifndef BOOST_DISABLE_THREADS
            recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
            
            pi_ = nullptr;
            
            return static_cast<root_ptr &>(base::operator = (nullptr));
        }
        
        template <typename V>
            root_ptr & operator = (V const * p)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
                
                pi_ = const_cast<V *>(p);
                
                return * this;
            }

        template <typename V, typename PoolAllocator>
            root_ptr & operator = (node<V, PoolAllocator> * p)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
                
                pi_ = static_cast<V *>(p->data());
                
                return static_cast<root_ptr &>(base::template operator = <V, PoolAllocator>(p));
            }
            
            root_ptr & operator = (root_ptr const & p)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
                
                pi_ = p.pi_;
                
                return static_cast<root_ptr &>(base::template operator = <void>(p));
            }

        root_ptr<root_ptr<void>> operator & () const
        {
            return root_ptr<root_ptr<void>>(base::proxy(), "", this);
        }
        
        operator bool () const
        {
            return pi_ != 0;
        }

        bool operator ! () const
        {
            return pi_ == 0;
        }

        operator void * () const
        {
            return pi_;
        }

        template <typename V>
            bool operator == (root_ptr<V> const & o) const
            {
                return pi_ == o.pi_;
            }

        template <typename V>
            bool operator != (root_ptr<V> const & o) const
            {
                return pi_ != o.pi_;
            }

            bool operator == (root_ptr<std::nullptr_t> const & o) const
            {
                return pi_ == nullptr;
            }

            bool operator != (root_ptr<std::nullptr_t> const & o) const
            {
                return pi_ != nullptr;
            }
            
        template <typename V>
            bool operator < (root_ptr<V> const & o) const
            {
                return pi_ < o.pi_;
            }

        template <typename V>
            bool operator > (root_ptr<V> const & o) const
            {
                return pi_ > o.pi_;
            }

        template <typename V>
            bool operator <= (root_ptr<V> const & o) const
            {
                return pi_ <= o.pi_;
            }

        template <typename V>
            bool operator >= (root_ptr<V> const & o) const
            {
                return pi_ >= o.pi_;
            }

        friend std::ostream & operator << (std::ostream & os, root_ptr const & o) 
        {
#ifndef BOOST_DISABLE_THREADS
            recursive_mutex::scoped_lock scoped_lock(static_mutex());
#endif
                
            return os << o.operator void * ();
        }

        ~root_ptr()
        {
            //if (po_ && ! cyclic() && get()->use_count() == 1)
            //    BOOSvoid_LOG_voidRIVIAL(info) << "\"" << pn_ << "\":" << boost::stacktrace::stacktrace(1, 1);
        }
    };


/**
    Construct new objects.
*/


template <typename T, bool = has_static_member_function___proxy<T, T const * (node_proxy &, T const *)>::value>
    struct construct;

    
template <typename T, size_t S>
    struct construct_array1
    {
        template <typename First, typename... Args>
            inline std::array<T, S> & operator () (node_proxy & __y, char const * n, std::array<T, S> & res, First && first, Args &&... args) const
            {
                new (& res[S - sizeof...(args) - 1]) T(construct<T>()(__y, n, first));
                
                return construct_array1<T, S>()(__y, n, res, std::forward<Args>(args)...);
            }
            
            inline std::array<T, S> & operator () (node_proxy & __y, char const * n, std::array<T, S> & res) const
            {
                return res;
            }
    };

template <typename T, size_t S, size_t I = 0>
    struct construct_array2
    {
        inline std::array<T, S> & operator () (node_proxy & __y, char const * n, std::array<T, S> & res, T const args[S]) const
        {
            new (& res[I]) T(construct<T>()(__y, n, args[I]));
            
            return construct_array2<T, S, I + 1>()(__y, n, res, args);
        }
    };

template <typename T, size_t S>
    struct construct_array2<T, S, S>
    {
        inline std::array<T, S> & operator () (node_proxy & __y, char const * n, std::array<T, S> & res, T const args[S]) const
        {
            return res;
        }
    };


template <typename T, size_t S, size_t I = 0>
    struct construct_array3
    {
        inline std::array<T, S> & operator () (node_proxy & __y, char const * n, std::array<T, S> & res, std::initializer_list<T> && args) const
        {
            new (& res[I]) T(construct<T>()(__y, n, * (args.begin() + I)));
            
            return construct_array3<T, S, I + 1>()(__y, n, res, std::forward<std::initializer_list<T>>(args));
        }
    };

template <typename T, size_t S>
    struct construct_array3<T, S, S>
    {
        inline std::array<T, S> & operator () (node_proxy & __y, char const * n, std::array<T, S> & res, std::initializer_list<T> && args) const
        {
            return res;
        }
    };


template <typename T, size_t S, size_t I = 0>
    struct construct_array4
    {
        inline std::array<T, S> & operator () (node_proxy & __y, char const * n, std::array<T, S> & res) const
        {
            new (& res[I]) T(construct<T>()(__y, n));
            
            return construct_array4<T, S, I + 1>()(__y, n, res);
        }
    };

template <typename T, size_t S>
    struct construct_array4<T, S, S>
    {
        inline std::array<T, S> & operator () (node_proxy & __y, char const * n, std::array<T, S> & res) const
        {
            return res;
        }
    };


template <typename T, bool>
    struct construct
    {
        template <typename... Args>
            inline T operator () (node_proxy & __y, char const * n, Args &&... args) const
            {
                return T(std::forward<Args>(args)...);
            }
    };

template <typename T, size_t S>
    struct construct<std::array<T, S>, false>
    {
        template <typename... Args>
        inline std::array<T, S> operator () (node_proxy & __y, char const * n, Args &&... args) const
        {
            typename std::aligned_storage<sizeof(std::array<T, S>), alignof(std::array<T, S>)>::type res;
            
            construct_array1<T, S>()(__y, n, reinterpret_cast<std::array<T, S> &>(res), std::forward<Args>(args)...);
            construct_array4<T, S, sizeof...(args)>()(__y, n, reinterpret_cast<std::array<T, S> &>(res));
            
            return reinterpret_cast<std::array<T, S> &>(res);
        }
        
        inline std::array<T, S> operator () (node_proxy & __y, char const * n, std::array<T, S> const & arg) const
        {
            return std::array<T, S>(arg);
        }
        
        inline std::array<T, S> operator () (node_proxy & __y, char const * n, T const args[S]) const
        {
            typename std::aligned_storage<sizeof(std::array<T, S>), alignof(std::array<T, S>)>::type res;
            
            return construct_array2<T, S>()(__y, n, reinterpret_cast<std::array<T, S> &>(res), args);
        }

        inline std::array<T, S> operator () (node_proxy & __y, char const * n, std::initializer_list<T> && args) const
        {
            typename std::aligned_storage<sizeof(std::array<T, S>), alignof(std::array<T, S>)>::type res;
            
            return construct_array3<T, S>()(__y, n, reinterpret_cast<std::array<T, S> &>(res), std::forward<std::initializer_list<T>>(args));
        }
    };
    
template <typename T, size_t S>
    struct construct<root_array<T, S>, false>
    {
        template <typename... Args>
            inline root_array<T, S> operator () (node_proxy & __y, char const * n, Args &&... args) const
            {
                return root_array<T, S>(__y, n, std::forward<Args>(args)...);
            }
    };

template <typename T>
    struct construct<root_ptr<T>, false>
    {
        template <typename... Args>
            inline root_ptr<T> operator () (node_proxy & __y, char const * n, Args &&... args) const
            {
                return root_ptr<T>(__y, n, std::forward<Args>(args)...);
            }
    };

template <typename T>
    struct construct<T, true>
    {
        template <typename... Args>
            inline T operator () (node_proxy & __y, char const * n, Args &&... args) const
            {
                return T(__y, std::forward<Args>(args)...);
            }
    };
    
template <typename T>
    inline T make_construct(node_proxy & __y, char const * n, T && po)
    {
        return construct<T>()(__y, n, std::forward<T>(po));
    }
    

/**
    Allocate new buffers;
*/

template <typename T>
    struct create
    {
        template <typename... Args>
            inline node<T> * operator () (node_proxy & __y, Args &&... args) const
            {
                return new node<T>(construct<T>()(__y, "",  std::forward<Args>(args)...));
            }
    };
    
template <typename T, size_t S>
    struct create_array
    {
        template <typename... Args>
            inline node<std::array<T, S>> * operator () (node_proxy & __y, Args &&... args) const
            {
                return new node<std::array<T, S>>(construct<std::array<T, S>>()(__y, "", std::forward<Args>(args)...));
            }
        
        inline node<std::array<T, S>> * from_copy(node_proxy & __y, char const * n, std::array<T, S> const & arg) const
        {
            return new node<std::array<T, S>>(construct<std::array<T, S>>()(__y, "", arg));
        }
        
        inline node<std::array<T, S>> * from_initializer(node_proxy & __y, std::initializer_list<T> && l) const
        {
            return new node<std::array<T, S>>(construct<std::array<T, S>>()(__y, "", std::forward<std::initializer_list<T>>(l)));
        }
    };

template <typename T>
    struct create_vector
    {
        template <typename... Args>
            inline node<std::vector<T>> * operator () (node_proxy & __y, size_t s, Args &&... args) const
            {
                return new node<std::vector<T>>(s, construct<T>()(__y, "", std::forward<Args>(args)...));
            }
            
        inline node<std::vector<T>> * from_range(node_proxy & __y, T const * begin, T const * end) const
        {
            return new node<std::vector<T>>(begin, end);
        }
        
        inline node<std::vector<T>> * from_initializer(node_proxy & __y, std::initializer_list<T> && l) const
        {
            return new node<std::vector<T>>(construct<std::vector<T>>()(__y, "", std::forward<std::initializer_list<T>>(l)));
        }
    };


template <typename T, size_t S>
    class root_array : public boost::root_ptr<T>
    {
        typedef boost::root_ptr<T> base;
        
    public:
        root_array(boost::node_proxy & __y, char const * n, T const pp[S]) 
        : base(__y, n, boost::create_array<T, S>()(__y, pp))
        {
        }
        
        root_array(boost::node_proxy & __y, char const * n, std::initializer_list<T> && pp) 
        : base(__y, n, boost::create_array<T, S>().from_initializer(__y, std::forward<std::initializer_list<T>>(pp)))
        {
        }
        
        template <typename... Args>
            root_array(boost::node_proxy & __y, char const * n, Args &&... args) 
            : base(__y, n, boost::create_array<T, S>()(__y,  std::forward<Args>(args)...))
            {
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
    Static cast.
*/

template <typename T, typename V>
    inline T static_pointer_cast(V const & p)
    {
        return T(p, "", static_cast_tag());
    }


/**
    Dynamic cast.
*/

template <typename T, typename V>
    inline T dynamic_pointer_cast(V const & p)
    {
        return T(p, "", dynamic_cast_tag());
    }

    
/**
    Reinterpret cast.
*/

template <typename T, typename V>
    inline T reinterpret_pointer_cast(V const & p)
    {
        return T(root_ptr<void>(p, "", static_cast_tag()), "", static_cast_tag());
    }

    
/**
    Const cast.
*/

template <typename T, typename V>
    inline T const_pointer_cast(V const & p)
    {
        return T(p);
    }



/**
    Comparison operator.

    @param  a1  Operand 1.
    @param  a2  Operand 2.
*/

template <typename T>
    inline bool operator == (root_proxy<T> const &a1, root_proxy<T> const &a2)
    {
        return a1.get() == a2.get();
    }


/**
    Comparison operator.

    @param  a1  Operand 1.
    @param  a2  Operand 2.
*/

template <typename T>
    inline bool operator != (root_proxy<T> const &a1, root_proxy<T> const &a2)
    {
        return a1.get() != a2.get();
    }


} // namespace boost


#if defined(_MSC_VER)
#pragma warning( pop )
#endif


#endif // #ifndef BOOST_NODE_PTR_INCLUDED
