/*!
    \file
    \brief Boost root_ptr.hpp mainheader file.

    Patent US11288049B2
    'SOURCE TO SOURCE COMPILER, COMPILATION METHOD, AND
    COMPUTER-READABLE MEDIUM FOR PREDICTABLE MEMORY MANAGEMENT'

    Copyright (C) 2020-2024 Fornux Inc.

    Phil Bouchard, Founder & CEO
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

#ifndef BOOST_NODE_PTR_INCLUDED
#define BOOST_NODE_PTR_INCLUDED

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4355 )

#include <new.h>
#endif

#include <cstdint>
#include <cstdlib>

#include <array>
#include <vector>
#include <atomic>
#include <limits>
#include <utility>
#include <sstream>
#include <initializer_list>

#ifndef BOOST_DISABLE_THREADS
#include <mutex>
#include <boost/thread/thread.hpp>
#include <boost/thread/recursive_mutex.hpp>
#endif

#include <iostream>
#include <boost/log/trivial.hpp>
#include <boost/tti/has_static_member_function.hpp>
#include <boost/smart_ptr/detail/intrusive_list.hpp>
#include <boost/smart_ptr/detail/intrusive_stack.hpp>
#include <boost/smart_ptr/detail/node_base.hpp>


BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION(__proxy);


namespace boost
{


struct node_base;


#ifndef BOOST_DISABLE_THREADS
/** Main global mutex used for thread safety */
static std::recursive_mutex & static_recursive_mutex()
{
    static std::recursive_mutex mutex_;

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
    node_proxy const * parent_;

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

    node_proxy(char const * file, char const * function, unsigned line, size_t depth = 0, node_proxy const * parent = nullptr) : file_(file), function_(function), line_(line), depth_(depth), parent_(parent), destroying_(false)
    {
        * top_node_proxy() = this;
    }


    static node_proxy const ** top_node_proxy()
    {
        static thread_local node_proxy const * p;

        return & p;
    }


    static std::ostream & stacktrace(std::ostream & out, node_proxy const * p)
    {
#if 1
        for (size_t depth = 0; p && p->depth_; ++ depth, p = p->parent_)
            out << '#' << depth << ' ' << p->function_ << " in " << p->file_ << " line " << p->line_<< '\n';
#endif

        return out;
    }


    /**
        Disabled copy constructor.
    */

    node_proxy(node_proxy const & x) = delete;


    /**
        Function-style access.
    */

    node_proxy const & operator () () const
    {
        return * this;
    }


    /**
        Destruction of a single @c node_proxy and detaching itself from other @c node_proxy .
    */

    virtual ~node_proxy()
    {
        reset();

        * top_node_proxy() = parent();
    }


    node_proxy const * parent() const
    {
        return parent_;
    }


    virtual uintptr_t depth() const
    {
        return reinterpret_cast<uintptr_t>(this);
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

    stack_node_proxy(char const * file, char const * function, unsigned line, node_proxy const & __y) : node_proxy(file, function, line, __y.depth() + 1, & __y)
    {
    }

    virtual uintptr_t depth() const
    {
        return ~ reinterpret_cast<uintptr_t>(this);
    }


    /**
        Destruction of a single @c stack_node_proxy .
     */

    virtual ~stack_node_proxy()
    {
    }
};


#ifdef BOOST_NO_EXCEPTIONS

void throw_exception(std::exception const & e)
{
    std::cerr << e.what() << "\n";
    node_proxy::stacktrace(std::cerr, * node_proxy::top_node_proxy());

    exit(-1);
}

#endif


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
        std::scoped_lock guard(static_recursive_mutex());
#endif

        reset(nullptr);
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
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

            reset(p);

            return * this;
        }

    root_core & operator = (root_core const & p)
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

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

protected:
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
        mutable node_proxy const * px_;


    public:
        typedef root_core base;


        /**
            Initialization of a pointer.

            @param  x   Reference to a @c node_proxy the pointer belongs to.
        */

        explicit root_proxy(node_proxy const & x)
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
            explicit root_proxy(node_proxy const & x, node<V, PoolAllocator> * p)
            : base(p)
            , px_(& x)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
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

        root_proxy & operator = (root_ptr<std::nullptr_t> const &)
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
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
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
            root_proxy & operator = (root_proxy<V> const & p)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                if (p.px_->depth() < px_->depth())
                    proxy(* p.px_);
                else if (px_->depth() < p.px_->depth())
                    p.proxy(* px_);

                base::operator = (static_cast<typename root_proxy<V>::base const &>(p));

                return * this;
            }


        /**
            Assignment.

            @param  p New pointer to manage.
        */

            root_proxy & operator = (root_proxy const & p)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                if (p.px_->depth() < px_->depth())
                    proxy(* p.px_);
                else if (px_->depth() < p.px_->depth())
                    p.proxy(* px_);

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

        node_proxy const & proxy()
        {
            return * px_;
        }


        /**
            Returns associated proxy.

            @return     @c node_proxy part of @c root_proxy .
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
            if (! px_ || x.depth() < px_->depth())
            {
                px_ = & x;

                if (base::get())
                {
                    base::get()->node_tag_.erase();
                    x.init(base::get());
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

        template <typename U, typename V> friend root_ptr<U> static_pointer_cast(root_ptr<V> const & p);
        template <typename U, typename V> friend root_ptr<U> dynamic_pointer_cast(root_ptr<V> const & p);
        template <typename U, typename V> friend root_ptr<U> reinterpret_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V const> const_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V> const_pointer_cast(root_ptr<V const> const & p);

    protected:
        typedef root_proxy<std::nullptr_t> base;

    protected:
        /** Iterator. */
        std::nullptr_t * pi_;
        
    public:
        typedef typename base::value_type value_type;


        root_ptr(node_proxy const & x, std::nullptr_t p)
        : base(x)
        , pi_(p)
        {
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
            operator V () const
            {
                return V(pi_);
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

#if 0
        friend std::ostream & operator << (std::ostream & os, root_ptr const & o)
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            return os << o.pi_;
        }
#endif

        ~root_ptr()
        {
#ifdef BOOST_REPORT
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            if (base::base::get() && ! base::cyclic() && base::base::get()->explicit_delete_ == false)
            {
                std::cerr << "report; memory leak; " << base::base::get()->size_bytes() << std::endl;
            }
#endif
        }
    };


template <typename T>
    class root_ptr : public root_proxy<T>
    {
        template <typename> friend class root_ptr;

        template <typename U, typename V> friend root_ptr<U> static_pointer_cast(root_ptr<V> const & p);
        template <typename U, typename V> friend root_ptr<U> dynamic_pointer_cast(root_ptr<V> const & p);
        template <typename U, typename V> friend root_ptr<U> reinterpret_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V const> const_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V> const_pointer_cast(root_ptr<V const> const & p);

    protected:
        typedef root_proxy<T> base;

    protected:
        /** Iterator. */
        T * pi_;

    public:
        typedef typename base::value_type value_type;


        root_ptr(root_ptr const & p)
        : base(p)
        , pi_(p.pi_)
        {
        }

        template <typename V>
            root_ptr(root_ptr<V> const & p)
            : base(p)
            , pi_(static_cast<T *>(static_cast<void *>(p.pi_)))
            {
            }

        template <typename V, typename... Args>
            root_ptr(root_ptr<V (Args...)> const & p)
            : base(p)
            , pi_(static_cast<T *>(static_cast<void *>(p.pi_)))
            {
            }

            root_ptr(root_ptr<std::nullptr_t> const & p)
            : base(p.proxy())
            , pi_(nullptr)
            {
            }

#if 0 //defined(BOOST_HAS_RVALUE_REFS)
        template <typename V>
            root_ptr(root_ptr<V> && p)
            : base(std::move(p))
            , pi_(std::move(p.pi_))
            {
            }
#endif

        root_ptr(node_proxy const & x)
        : base(x)
        , pi_(nullptr)
        {
        }

        template <size_t N>
            root_ptr(node_proxy const & x, T (& p)[N])
            : base(x)
            , pi_(p)
            {
            }

        root_ptr(node_proxy const & x, root_ptr<std::nullptr_t> const & p)
        : base(x)
        , pi_(p)
        {
        }

        root_ptr(node_proxy const & x, std::uintptr_t p)
        : base(x)
        , pi_(reinterpret_cast<T *>(p))
        {
        }

        template <typename V>
            root_ptr(node_proxy const & x, V * p)
            : base(x)
            , pi_(static_cast<T *>(static_cast<void *>(p)))
            {
            }

        template <typename V, typename... Args>
            root_ptr(node_proxy const & x, V (* p)(Args...))
            : base(x)
            , pi_(static_cast<T *>(static_cast<void *>(p)))
            {
            }

        template <typename V>
            root_ptr(node_proxy const & x, V const * p)
            : base(x)
            , pi_(static_cast<T *>(static_cast<void *>(const_cast<V *>(p))))
            {
            }

        template <typename V, typename... Args>
            root_ptr(node_proxy const & x, V const (* p)(Args...))
            : base(x)
            , pi_(static_cast<T *>(static_cast<void *>(const_cast<V *>(p))))
            {
            }

        template <typename V, typename PoolAllocator>
            root_ptr(node_proxy const & x, node<V, PoolAllocator> * p)
            : base(x, p)
            , pi_(static_cast<T *>(const_cast<void *>(p->data())))
            {
            }

            root_ptr(node_proxy const & x, root_ptr const & p)
            : base(p)
            , pi_(p.pi_)
            {
            }

        template <typename V>
            root_ptr(node_proxy const & x, root_ptr<V> const & p)
            : base(p)
            , pi_(static_cast<T *>(static_cast<void *>(p.pi_)))
            {
            }

        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            root_ptr(root_ptr<V> const & p, static_cast_tag const & t)
            : base(p)
            , pi_(static_cast<T *>(p.pi_))
            {
#ifndef BOOST_NO_EXCEPTIONS
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                if (! pi_)
                {
                    std::stringstream out;
                    out << "internal error\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }
#endif
            }

        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            root_ptr(root_ptr<V> const & p, dynamic_cast_tag const & t)
            : base(p)
            , pi_(dynamic_cast<T *>(p.pi_))
            {
#ifndef BOOST_NO_EXCEPTIONS
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                if (! pi_)
                {
                    std::stringstream out;
                    out << "internal error\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }
#endif
            }

        root_ptr & operator = (root_ptr<std::nullptr_t> const & p)
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            pi_ = nullptr;

            return static_cast<root_ptr &>(base::operator = (p));
        }

        template <size_t N>
            root_ptr & operator = (T (& p)[N])
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                pi_ = p;

                return * this;
            }

        template <typename V, typename PoolAllocator>
            root_ptr & operator = (node<V, PoolAllocator> * p)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                pi_ = static_cast<V *>(const_cast<void *>(p->data()));

                return static_cast<root_ptr &>(base::template operator = <V, PoolAllocator>(p));
            }

        template <typename V>
            root_ptr & operator = (root_ptr<V> const & p)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                pi_ = p.pi_;

                return static_cast<root_ptr &>(base::template operator = <V>(p));
            }

            root_ptr & operator = (root_ptr const & p)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                pi_ = p.pi_;

                return static_cast<root_ptr &>(base::operator = (p));
            }

        template <typename V>
            T & operator [] (V n)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

#ifdef BOOST_REPORT
                if (base::base::get() && base::base::get()->explicit_delete_ == true)
                {
                    std::cerr << "report; use after free; 1 <<std::endl;
                }
#endif

#ifdef BOOST_REPORT
                if (base::base::get() && base::base::get()->size() <= n)
                {
                    std::cerr << "report; out of bounds; " << 1 << std::endl;
                }
#endif

#ifndef BOOST_NO_EXCEPTIONS
                if (! pi_)
                {
                    std::stringstream out;
                    out << "null pointer\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }

#if 0
                if (base::base::get() && base::base::get()->size() <= n)
                {
                    std::stringstream out;
                    out << "out of range [0, " << base::base::get()->size() << "[\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }
#endif
#endif

                return * (pi_ + n);
            }

        template <typename V>
            T const & operator [] (V n) const
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

#ifdef BOOST_REPORT
                if (base::base::get() && base::base::get()->explicit_delete_ == true)
                {
                    std::cerr << "report; use after free; " << 1 << std::endl;
                }
#endif

#ifdef BOOST_REPORT
                if (base::base::get() && base::base::get()->size() <= n)
                {
                    std::cerr << "report; out of bounds; " << 1 << std::endl;
                }
#endif

#ifndef BOOST_NO_EXCEPTIONS
                if (! pi_)
                {
                    std::stringstream out;
                    out << "null pointer\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }

#if 0
                if (base::base::get() && base::base::get()->size() <= n)
                {
                    std::stringstream out;
                    out << "(" << n << ") is out of range [0, " << base::base::get()->size() << "[\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }
#endif
#endif

                return * (pi_ + n);
            }

        T & operator * () const
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

#ifdef BOOST_REPORT
            if (base::base::get() && base::base::get()->explicit_delete_ == true)
            {
                std::cerr << "report; use after free; " << 1 << std::endl;
            }
#endif

#ifdef BOOST_REPORT
            if (base::base::get() && (base::base::get()->size() == 0 || pi_ < static_cast<T *>(const_cast<void *>(base::base::get()->data())) || pi_ >= static_cast<T *>(const_cast<void *>(base::base::get()->data())) + base::base::get()->size()))
            {
                std::cerr << "report; out of bounds; " << 1 << std::endl;
            }
#endif

#ifndef BOOST_NO_EXCEPTIONS
            if (! pi_)
            {
                std::stringstream out;
                out << "null pointer\n";
                node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                throw std::out_of_range(out.str());
            }

#if 0
            if (base::base::get() && (base::base::get()->size() == 0 || pi_ < static_cast<T *>(const_cast<void *>(base::base::get()->data())) || pi_ >= static_cast<T *>(const_cast<void *>(base::base::get()->data())) + base::base::get()->size()))
            {
                std::stringstream out;
                out << "(" << pi_ - static_cast<T *>(const_cast<void *>(base::base::get()->data())) << ") is out of range [0, " << base::base::get()->size() << "[\n";
                node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                throw std::out_of_range(out.str());
            }
#endif
#endif

            return * pi_;
        }

        T * operator -> () const
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

#ifdef BOOST_REPORT
            if (base::base::get() && base::base::get()->explicit_delete_ == true)
            {
                std::cerr << "report; use after free; " << 1 << std::endl;
            }
#endif

#ifdef BOOST_REPORT
            if (base::base::get() && (base::base::get()->size() == 0 || pi_ < static_cast<T *>(const_cast<void *>(base::base::get()->data())) || pi_ >= static_cast<T *>(const_cast<void *>(base::base::get()->data())) + base::base::get()->size()))
            {
                std::cerr << "report; out of bounds; " << 1 << std::endl;
            }
#endif

#ifndef BOOST_NO_EXCEPTIONS
            if (! pi_)
            {
                std::stringstream out;
                out << "null pointer\n";
                node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                throw std::out_of_range(out.str());
            }

#if 0
            if (base::base::get() && (base::base::get()->size() == 0 || pi_ < static_cast<T *>(const_cast<void *>(base::base::get()->data())) || pi_ >= static_cast<T *>(const_cast<void *>(base::base::get()->data())) + base::base::get()->size()))
            {
                std::stringstream out;
                out << "(" << pi_ - static_cast<T *>(const_cast<void *>(base::base::get()->data())) << ") is out of range [0, " << base::base::get()->size() << "[\n";
                node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                throw std::out_of_range(out.str());
            }
#endif
#endif

            return pi_;
        }

#if 0
        root_ptr<root_ptr<T>> operator & () const
        {
            return root_ptr<root_ptr<T>>(base::proxy(), this);
        }
#endif

#if 0
        operator bool () const
        {
            return pi_ != 0;
        }
#endif

        bool operator ! () const
        {
            return pi_ == 0;
        }

#if 1
        operator T * ()
        {
#ifdef BOOST_REPORT
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            if (base::base::get() && base::base::get()->explicit_delete_ == true)
            {
                std::cerr << "report; use after free; " << 1 << std::endl;
            }
#endif

            return pi_;
        }

        operator T const * () const
        {
#ifdef BOOST_REPORT
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            if (base::base::get() && base::base::get()->explicit_delete_ == true)
            {
                std::cerr << "report; use after free; " << 1 << std::endl;
            }
#endif

            return pi_;
        }
#endif

#if 0
        template <typename V>
            operator V const * () const
            {
#ifdef BOOST_REPORT
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                if (base::base::get() && base::base::get()->explicit_delete_ == true)
                {
                    std::cerr << "report; use after free; " << 1 << std::endl;
                }
#endif

                return reinterpret_cast<V const *>(pi_);
            }
#endif

#if 0
        template <typename V>
            operator root_ptr<V> () const
            {
#ifdef BOOST_REPORT
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                if (base::base::get() && base::base::get()->explicit_delete_ == true)
                {
                    std::cerr << "report; use after free; " << 1 << std::endl;
                }
#endif

                return root_ptr<V>(base::proxy(), reinterpret_cast<V>(pi_));
            }

#if 1
        template <typename V, typename... Args>
            operator root_ptr<V (Args...)> () const
            {
#ifdef BOOST_REPORT
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                if (base::base::get() && base::base::get()->explicit_delete_ == true)
                {
                    std::cerr << "report; use after free; " << 1 << std::endl;
                }
#endif

                return root_ptr<V (Args...)>(base::proxy(), reinterpret_cast<V (*)(Args...)>(pi_));
            }
#endif
#endif

#if 0
        operator uintptr_t () const
        {
#ifdef BOOST_REPORT
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            if (base::base::get() && base::base::get()->explicit_delete_ == true)
            {
                std::cerr << "report; use after free; " << 1 << std::endl;
            }
#endif

            return reinterpret_cast<uintptr_t>(pi_);
        }
#endif

        root_ptr & operator ++ ()
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            return ++ pi_, * this;
        }

        root_ptr & operator -- ()
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            return -- pi_, * this;
        }

        root_ptr operator ++ (int)
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            root_ptr temp(* this);

            return ++ pi_, temp;
        }

        root_ptr operator -- (int)
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            root_ptr temp(* this);

            return -- pi_, temp;
        }

        ptrdiff_t operator + (root_ptr const & o) const
        {
            return pi_ + o.pi_;
        }

        ptrdiff_t operator - (root_ptr const & o) const
        {
            return pi_ - o.pi_;
        }

        template <typename V>
            root_ptr operator + (V i) const
            {
                return root_ptr(base::proxy(), pi_ + i);
            }

        template <typename V>
            root_ptr operator - (V i) const
            {
                return root_ptr(base::proxy(), pi_ - i);
            }

        template <typename V>
            root_ptr & operator += (V i)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                pi_ += i;

                return * this;
            }

        template <typename V>
            root_ptr & operator -= (V i)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
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

#if 0
        friend std::ostream & operator << (std::ostream & os, root_ptr const & o)
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            return os << o.pi_;
        }
#endif

        ~root_ptr()
        {
#ifdef BOOST_REPORT
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            if (base::base::get() && ! base::cyclic() && base::base::get()->explicit_delete_ == false)
            {
                std::cerr << "report; memory leak; " << base::base::get()->size_bytes() << std::endl;
            }
#endif
        }
    };
    
    
#if 1
template <typename T>
    class root_ptr<const T> : public root_ptr<T>
    {
    public:
        using root_ptr<T>::root_ptr;
        
        root_ptr(root_ptr<T> const & p)
        : root_ptr<T>(p)
        {
        }
    };
#endif


template <>
    class root_ptr<void> : public root_proxy<void>
    {
        template <typename> friend class root_ptr;

        template <typename U, typename V> friend root_ptr<U> static_pointer_cast(root_ptr<V> const & p);
        template <typename U, typename V> friend root_ptr<U> dynamic_pointer_cast(root_ptr<V> const & p);
        template <typename U, typename V> friend root_ptr<U> reinterpret_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V const> const_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V> const_pointer_cast(root_ptr<V const> const & p);

    protected:
        typedef root_proxy<void> base;

    protected:
        /** Iterator. */
        void * pi_;

    public:
        typedef typename base::value_type value_type;


        root_ptr(root_ptr const & p)
        : base(p)
        , pi_(p.pi_)
        {
        }

        template <typename V>
            root_ptr(root_ptr<V> const & p)
            : base(p)
            , pi_(p.pi_)
            {
            }

            root_ptr(root_ptr<std::nullptr_t> const & p)
            : base(p.proxy())
            , pi_(nullptr)
            {
            }

#if 0 //defined(BOOST_HAS_RVALUE_REFS)
        template <typename V>
            root_ptr(root_ptr<V> && p)
            : base(std::move(p))
            , pi_(std::move(p.pi_))
            {
            }
#endif

        root_ptr(node_proxy const & x)
        : base(x)
        , pi_(nullptr)
        {
        }

        root_ptr(node_proxy const & x, root_ptr<std::nullptr_t> const & p)
        : base(x)
        , pi_(p)
        {
        }

        root_ptr(node_proxy const & x, std::uintptr_t p)
        : base(x)
        , pi_(reinterpret_cast<void *>(p))
        {
        }

        template <typename V>
            root_ptr(node_proxy const & x, V * p)
            : base(x)
            , pi_(p)
            {
            }

        template <typename V>
            root_ptr(node_proxy const & x, V const * p)
            : base(x)
            , pi_(const_cast<V *>(p))
            {
            }

        template <typename V, typename PoolAllocator>
            root_ptr(node_proxy const & x, node<V, PoolAllocator> * p)
            : base(x, p)
            , pi_(static_cast<V *>(p->data()))
            {
            }

        template <typename V>
            root_ptr(node_proxy const & x, root_ptr<V> const & p)
            : base(p)
            , pi_(p.pi_)
            {
            }

            root_ptr(node_proxy const & x, root_ptr const & p)
            : base(p)
            , pi_(p.pi_)
            {
            }

        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            root_ptr(root_ptr<V> const & p, static_cast_tag const & t)
            : base(p)
            , pi_(static_cast<void *>(p.pi_))
            {
            }


        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            root_ptr(root_ptr<V> const & p, dynamic_cast_tag const & t)
            : base(p)
            , pi_(dynamic_cast<void *>(p.pi_))
            {
            }


        root_ptr & operator = (root_ptr<std::nullptr_t> const & p)
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            pi_ = nullptr;

            return static_cast<root_ptr &>(base::operator = (p));
        }

        template <typename V, typename PoolAllocator>
            root_ptr & operator = (node<V, PoolAllocator> * p)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                pi_ = static_cast<V *>(p->data());

                return static_cast<root_ptr &>(base::template operator = <V, PoolAllocator>(p));
            }

            root_ptr & operator = (root_ptr const & p)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                pi_ = p.pi_;

                return static_cast<root_ptr &>(base::template operator = <void>(p));
            }

#if 0
        root_ptr<root_ptr<void>> operator & () const
        {
            return root_ptr<root_ptr<void>>(base::proxy(), this);
        }
#endif

#if 0
        operator bool () const
        {
            return pi_ != 0;
        }
#endif

        bool operator ! () const
        {
            return pi_ == 0;
        }

#if 1
        operator void * ()
        {
            return pi_;
        }

        operator void const * () const
        {
            return pi_;
        }
#endif

#if 1
        operator uintptr_t () const
        {
#ifdef BOOST_REPORT
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            if (base::base::get() && base::base::get()->explicit_delete_ == true)
            {
                std::cerr << "report; use after free; " << 1 << std::endl;
            }
#endif

            return reinterpret_cast<uintptr_t>(pi_);
        }
#endif

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

#if 0
        friend std::ostream & operator << (std::ostream & os, root_ptr const & o)
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            return os << o.pi_;
        }
#endif

        ~root_ptr()
        {
#ifdef BOOST_REPORT
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            if (base::base::get() && ! base::cyclic() && base::base::get()->explicit_delete_ == false)
            {
                std::cerr << "report; memory leak; " << base::base::get()->size_bytes() << std::endl;
            }
#endif
        }
    };


#if 1
template <>
    class root_ptr<const void> : public root_ptr<void>
    {
    public:
        using root_ptr<void>::root_ptr;
        
        root_ptr(root_ptr<void> const & p)
        : root_ptr<void>(p)
        {
        }
    };
#endif


#if 0
template <typename T, typename... Args>
    class root_ptr<T (Args...)> : public root_proxy<T (Args...)>
    {
        template <typename> friend class root_ptr;

        template <typename U, typename V> friend U static_pointer_cast(V const & p);
        template <typename U, typename V> friend U dynamic_pointer_cast(V const & p);
        template <typename U, typename V> friend U reinterpret_pointer_cast(V const & p);
        template <typename V> friend root_ptr<V const> const_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V> const_pointer_cast(root_ptr<V const> const & p);

    protected:
        typedef root_proxy<T (Args...)> base;

    public:
        using element_type = T (Args...);
        using value_type = typename base::value_type;

    protected:
        /** Iterator. */
        element_type * pi_;

    public:
        root_ptr(root_ptr const & p)
        : base(p)
        , pi_(p.pi_)
        {
        }

        template <typename V>
            root_ptr(root_ptr<V> const & p)
            : base(p)
            , pi_(p.pi_)
            {
            }

            root_ptr(root_ptr<std::nullptr_t> const & p)
            : base(p.proxy())
            , pi_(nullptr)
            {
            }

#if 0 //defined(BOOST_HAS_RVALUE_REFS)
        template <typename V>
            root_ptr(root_ptr<V> && p)
            : base(std::move(p))
            , pi_(std::move(p.pi_))
            {
            }
#endif

        root_ptr(node_proxy const & x)
        : base(x)
        , pi_(nullptr)
        {
        }

        root_ptr(node_proxy const & x, root_ptr<std::nullptr_t> const & p)
        : base(x)
        , pi_(p)
        {
        }

        root_ptr(node_proxy const & x, std::uintptr_t p)
        : base(x)
        , pi_(reinterpret_cast<void *>(p))
        {
        }

        template <typename V>
            root_ptr(node_proxy const & x, V * p)
            : base(x)
            , pi_(p)
            {
            }

        template <typename V>
            root_ptr(node_proxy const & x, V const * p)
            : base(x)
            , pi_(const_cast<V *>(p))
            {
            }

        template <typename V, typename PoolAllocator>
            root_ptr(node_proxy const & x, node<V, PoolAllocator> * p)
            : base(x, p)
            , pi_(static_cast<V *>(p->data()))
            {
            }

        template <typename V>
            root_ptr(node_proxy const & x, root_ptr<V> const & p)
            : base(p)
            , pi_(p.pi_)
            {
            }

            root_ptr(node_proxy const & x, root_ptr const & p)
            : base(p)
            , pi_(p.pi_)
            {
            }

        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            root_ptr(root_ptr<V> const & p, static_cast_tag const & t)
            : base(p)
            , pi_(static_cast<T (Args...)>(p.pi_))
            {
            }


        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            root_ptr(root_ptr<V> const & p, dynamic_cast_tag const & t)
            : base(p)
            , pi_(dynamic_cast<T (Args...)>(p.pi_))
            {
            }


        root_ptr & operator = (root_ptr<std::nullptr_t> const & p)
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            pi_ = nullptr;

            return static_cast<root_ptr &>(base::operator = (p));
        }

        template <size_t S>
            root_ptr & operator = (T (p[S])(Args...))
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                pi_ = p;

                return * this;
            }

        template <typename V, typename PoolAllocator>
            root_ptr & operator = (node<V, PoolAllocator> * p)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                pi_ = static_cast<V *>(p->data());

                return static_cast<root_ptr &>(base::template operator = <V, PoolAllocator>(p));
            }

            root_ptr & operator = (root_ptr const & p)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                pi_ = p.pi_;

                return static_cast<root_ptr &>(base::template operator = <T (Args...)>(p));
            }

#if 0
        root_ptr<root_ptr<void>> operator & () const
        {
            return root_ptr<root_ptr<void>>(base::proxy(), this);
        }
#endif

        operator bool () const
        {
            return pi_ != 0;
        }

        bool operator ! () const
        {
            return pi_ == 0;
        }

        operator element_type * () const
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

#if 0
        friend std::ostream & operator << (std::ostream & os, root_ptr const & o)
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            return os << o.pi_;
        }
#endif

        ~root_ptr()
        {
#ifdef BOOST_REPORT
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            if (base::base::get() && ! base::cyclic() && base::base::get()->explicit_delete_ == false)
            {
                std::cerr << "report; memory leak; " << base::base::get()->size_bytes() << std::endl;
            }
#endif
        }
    };
#endif


/**
    Allocate new buffers;
*/


template <typename T, size_t S>
    class root_array : public boost::root_ptr<T>
    {
    protected:
        typedef boost::root_ptr<T> base;

        using base::pi_;

    public:
        root_array(boost::node_proxy const & x)
            : base(x)
        {
        }

#if 0
        root_array(boost::node_proxy const & x, T (& p)[S])
            : base(x, p)
        {
        }

        root_array(boost::node_proxy const & x, T (&& p)[S])
            : base(x, std::move(p))
        {
        }
#endif

        template <typename PoolAllocator>
            root_array(node_proxy const & x, node<std::array<T, S>, PoolAllocator> * p)
                : base(x, p)
            {
            }

        template <typename V>
            T & operator [] (V const n)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

#ifdef BOOST_REPORT
                if (S <= n)
                {
                    std::cerr << "report; out of bounds; " << 1 << std::endl;
                }
#endif

#ifndef BOOST_NO_EXCEPTIONS
                if (S <= n)
                {
                    std::stringstream out;
                    out << "out of range [0, " << S << "[\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }
#endif

                return * (pi_ + n);
            }

        template <typename V>
            T const & operator [] (V const n) const
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

#ifdef BOOST_REPORT
                if (S <= n)
                {
                    std::cerr << "report; out of bounds; " << 1 << std::endl;
                }
#endif

#ifndef BOOST_NO_EXCEPTIONS
                if (S <= n)
                {
                    std::stringstream out;
                    out << "out of range [0, " << S << "[\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }
#endif

                return * (pi_ + n);
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
    inline size_t constexpr size_of(T const &)
    {
        return sizeof(T);
    }

template <typename T, size_t S>
    inline size_t constexpr size_of(root_array<T, S> const &)
    {
        return sizeof(T) * S;
    }


/**
    Static cast.
*/

template <typename T, typename V>
    inline T static_pointer_cast(V const & p)
    {
        return T(p, static_cast_tag());
    }


/**
    Dynamic cast.
*/

template <typename T, typename V>
    inline T dynamic_pointer_cast(V const & p)
    {
        return T(p, dynamic_cast_tag());
    }


/**
    Reinterpret cast.
*/

template <typename T, typename V>
    inline T reinterpret_pointer_cast(V const & p)
    {
        return T(root_ptr<void>(p, static_cast_tag()), static_cast_tag());
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


/**
    Generic get_value().
*/

template <typename T>
    inline T const & get_value(T const & source)
    {
        return source;
    }

template <typename T>
    inline T get_value(std::atomic<T> const & source)
    {
        return std::atomic<T>(source.load());
    }


} // namespace boost


namespace std
{

template <typename T>
    struct hash<boost::root_ptr<T>>
    {
        size_t operator() (boost::root_ptr<T> const & p) const
        {
            return p.get();
        }
    };

template <typename T>
    struct equal_to<boost::root_ptr<T>>
    {
        bool operator() (boost::root_ptr<T> const & lhs, boost::root_ptr<T> const & rhs) const
        {
            return lhs.get() == rhs.get();
        }
    };

} // namespace std


#if defined(_MSC_VER)
#pragma warning( pop )
#endif


#endif // #ifndef BOOST_NODE_PTR_INCLUDED
