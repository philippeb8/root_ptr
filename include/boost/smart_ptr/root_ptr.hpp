/*!
    \file
    \brief Boost root_ptr.hpp mainheader file.

    Patent US11288049B2
    'SOURCE TO SOURCE COMPILER, COMPILATION METHOD, AND
    COMPUTER-READABLE MEDIUM FOR PREDICTABLE MEMORY MANAGEMENT'

    Copyright (C) 2020-2026 Fornux LLC

    Phil Bouchard, Founder & CEO
    Fornux LLC
    phil@fornux.com
    3909 S Maryland Pkwy Ste 314 #638, Las Vegas, NV, 89119

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
#include <boost/smart_ptr/detail/node_base.hpp>


namespace boost
{


struct node_base;
struct root_core;


#ifndef BOOST_DISABLE_THREADS
/** Main global mutex used for thread safety */
static inline std::recursive_mutex & static_recursive_mutex()
{
    static std::recursive_mutex mutex_;

    return mutex_;
}
#endif


/**
    Set header.

    Proxy object used to link a list of @c node<> blocks and a list of @c node_proxy .
*/

struct node_proxy
{
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

    /** List of all pointer instances belonging to a @c node_proxy . */
    mutable smart_ptr::detail::intrusive_list root_set_;


    /**
        Initialization of a single @c node_proxy .
    */

    node_proxy(char const * file, char const * function, unsigned line, node_proxy const * parent = nullptr, size_t depth = 0) : file_(file), function_(function), line_(line), parent_(parent), depth_(parent ? parent->depth_ + 1 : 0), destroying_(false)
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        * top_node_proxy() = this;
    }


    static node_proxy const ** top_node_proxy()
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        static thread_local node_proxy const * p;

        return & p;
    }


    static std::ostream & stacktrace(std::ostream & out, node_proxy const * p)
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        for (size_t depth = 0; p && p->depth_; ++ depth, p = p->parent_)
            out << '#' << depth << ' ' << p->function_ << " in " << p->file_ << " line " << p->line_<< '\n';

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

    ~node_proxy()
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        reset();

        * top_node_proxy() = parent();
    }


    node_proxy const * parent() const
    {
        return parent_;
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
        Get rid or delegate a series of @c node_proxy .
    */

    void reset();
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


struct root_core
{
    typedef node_base value_type;

    value_type * po_;
    void const * pi_;

    /** Enlists the @c node_proxy node @c root_core belongs to. */
    mutable smart_ptr::detail::intrusive_list root_tag_;


    explicit root_core(node_proxy const & x)
    : po_(nullptr)
    , pi_(nullptr)
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        x.root_set_.push_back(& root_tag_);
    }

    template <typename V, typename PoolAllocator>
        explicit root_core(node_proxy const & x, node<V, PoolAllocator> * p)
        : po_(p)
        , pi_(p->data())
        {
            using namespace smart_ptr::detail;
            
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            x.root_set_.push_back(& root_tag_);
        }
        
    template <typename V>
        explicit root_core(node_proxy const & x, V * p)
        : po_(nullptr)
        , pi_(p)
        {
            using namespace smart_ptr::detail;
            
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            x.root_set_.push_back(& root_tag_);
        }



    /**
        Initialization of a pointer.

        @param  p New pointer to manage.
    */

    root_core(root_core const & p)
    : po_(p.share())
    , pi_(p.pi_)
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        root_tag_.push_back(& p.root_tag_);
    }

    ~root_core()
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        reset(nullptr);
    }

#if defined(BOOST_HAS_RVALUE_REFS)
    root_core(root_core && p)
    : po_(std::exchange(p.po_, nullptr))
    , pi_(std::exchange(p.pi_, nullptr))
    {
    }
#endif

    template <typename V, typename PoolAllocator>
        root_core & operator = (node<V, PoolAllocator> * p)
        {
            using namespace smart_ptr::detail;
            
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            reset(p);
            
            pi_ = p->data();

            return * this;
        }


    /**
        Assignment.

        @param  p New pointer to manage.
    */

    root_core & operator = (root_core const & p)
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        reset(p.share());

        pi_ = p.pi_;

        root_tag_.push_back(& p.root_tag_);

        return * this;
    }
    

    value_type * get() const
    {
        return po_;
    }

    value_type * share() const
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        if (po_)
        {          
            po_->add_ref_copy();
        }

        return po_;
    }

    void reset(value_type * p = nullptr)
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        if (po_)
        {
            po_->release();
        }

        po_ = p;
    }
};


inline void node_proxy::reset()
{
    using namespace smart_ptr::detail;

#ifndef BOOST_DISABLE_THREADS
    std::scoped_lock guard(static_recursive_mutex());
#endif

    {
        // destroy cycles remaining
        if (! destroying() && ! root_set_.empty())
        {
            destroying(true);

            for (intrusive_list::iterator<root_core, & root_core::root_tag_> p = root_set_.begin(), q = root_set_.begin(); ++ q, p != q && p != root_set_.end(); p = q)
            {
                if (root_core::value_type * i = p->po_)
                {
                    p->po_ = nullptr;
                    p->pi_ = nullptr;

                    i->destroy();
                }
            }

            destroying(false);
        }
    }
}


template <typename T>
    class root_ptr;


template <>
    class root_ptr<std::nullptr_t> : protected root_core
    {
        template <typename> friend class root_ptr;

        template <typename U, typename V> friend root_ptr<U> static_pointer_cast(root_ptr<V> const & p);
        template <typename U, typename V> friend root_ptr<U> dynamic_pointer_cast(root_ptr<V> const & p);
        template <typename U, typename V> friend root_ptr<U> reinterpret_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V const> const_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V> const_pointer_cast(root_ptr<V const> const & p);

    protected:
        typedef root_core base;

    public:
        typedef typename base::value_type value_type;


        root_ptr(node_proxy const & x, std::nullptr_t p)
        : base(x)
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

        operator std::nullptr_t const * () const
        {
            return static_cast<std::nullptr_t const *>(pi_);
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

            if (base::get() && base::get()->explicit_delete_ == false)
            {
                std::cerr << "report; memory leak; " << base::get()->size_bytes() << std::endl;
            }
#endif
        }
    };


template <typename T>
    class root_ptr : protected root_core
    {
        template <typename> friend class root_ptr;

        template <typename U, typename V> friend root_ptr<U> static_pointer_cast(root_ptr<V> const & p);
        template <typename U, typename V> friend root_ptr<U> dynamic_pointer_cast(root_ptr<V> const & p);
        template <typename U, typename V> friend root_ptr<U> reinterpret_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V const> const_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V> const_pointer_cast(root_ptr<V const> const & p);

    protected:
        typedef root_core base;

    public:
        typedef typename base::value_type value_type;


        root_ptr(root_ptr const & p)
        : base(p)
        {
        }

        template <typename V>
            root_ptr(root_ptr<V> const & p)
            : base(p)
            {
            }

        template <typename V, typename... Args>
            root_ptr(root_ptr<V (Args...)> const & p)
            : base(p)
            {
            }

            root_ptr(root_ptr<std::nullptr_t> const & p)
            : base(p)
            {
            }

#if defined(BOOST_HAS_RVALUE_REFS)
        template <typename V>
            root_ptr(root_ptr<V> && p)
            : base(std::move(p))
            {
            }
#endif

        root_ptr(node_proxy const & x)
        : base(x)
        {
        }

#if 0
        template <size_t N>
            root_ptr(node_proxy const & x, T (& p)[N])
            : base(x)
            , pi_(p)
            {
            }
#endif

        root_ptr(node_proxy const & x, std::uintptr_t p)
        : base(x, reinterpret_cast<T *>(p))
        {
        }

#if 1
        template <typename V>
            root_ptr(node_proxy const & x, V * p)
            : base(x, p)
            {
            }

        template <typename V, typename... Args>
            root_ptr(node_proxy const & x, V (* p)(Args...))
            : base(x, p)
            {
            }

        template <typename V>
            root_ptr(node_proxy const & x, V const * p)
            : base(x, p)
            {
            }

        template <typename V, typename... Args>
            root_ptr(node_proxy const & x, V const (* p)(Args...))
            : base(x, p)
            {
            }
#endif

        template <typename V, typename PoolAllocator>
            root_ptr(node_proxy const & x, node<V, PoolAllocator> * p)
            : base(x, p)
            {
            }


            root_ptr(node_proxy const & x, root_ptr const & p)
            : base(p)
            {
            }


        template <typename V>
            root_ptr(node_proxy const & x, root_ptr<V> const & p)
            : base(p)
            {
            }


        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            root_ptr(root_ptr<V> const & p, static_cast_tag const & t)
            : base(p, static_cast<T *>(p.pi_))
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
            : base(p, dynamic_cast<T *>(p.pi_))
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

            return static_cast<root_ptr &>(base::operator = (p));
        }

#if 0
        template <size_t N>
            root_ptr & operator = (T (& p)[N])
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                pi_ = p;

                return * this;
            }
#endif

        template <typename V, typename PoolAllocator>
            root_ptr & operator = (node<V, PoolAllocator> * p)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                return static_cast<root_ptr &>(base::template operator = <V, PoolAllocator>(p));
            }

        template <typename V>
            root_ptr & operator = (root_ptr<V> const & p)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                return static_cast<root_ptr &>(base::template operator = <V>(p));
            }

            root_ptr & operator = (root_ptr const & p)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                return static_cast<root_ptr &>(base::operator = (p));
            }

        T & operator * () const
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

#ifdef BOOST_REPORT
            if (base::get() && base::get()->explicit_delete_ == true)
            {
                std::cerr << "report; use after free; " << 1 << std::endl;
            }
#endif

#ifdef BOOST_REPORT
            if (base::get() && (base::get()->size() == 0 || pi_ < static_cast<T *>(const_cast<void *>(base::get()->data())) || pi_ >= static_cast<T *>(const_cast<void *>(base::get()->data())) + base::get()->size()))
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
#endif

            return * static_cast<T *>(const_cast<void *>(pi_));
        }

        T * operator -> ()
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

#ifdef BOOST_REPORT
            if (base::get() && base::get()->explicit_delete_ == true)
            {
                std::cerr << "report; use after free; " << 1 << std::endl;
            }
#endif

#ifdef BOOST_REPORT
            if (base::get() && (base::get()->size() == 0 || pi_ < static_cast<T *>(const_cast<void *>(base::get()->data())) || pi_ >= static_cast<T *>(const_cast<void *>(base::get()->data())) + base::get()->size()))
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
#endif

            return static_cast<T *>(const_cast<void *>(pi_));
        }

        T const * operator -> () const
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

#ifdef BOOST_REPORT
            if (base::get() && base::get()->explicit_delete_ == true)
            {
                std::cerr << "report; use after free; " << 1 << std::endl;
            }
#endif

#ifdef BOOST_REPORT
            if (base::get() && (base::get()->size() == 0 || pi_ < static_cast<T *>(const_cast<void *>(base::get()->data())) || pi_ >= static_cast<T *>(const_cast<void *>(base::get()->data())) + base::get()->size()))
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
#endif

            return static_cast<T const *>(pi_);
        }

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

            if (base::get() && base::get()->explicit_delete_ == true)
            {
                std::cerr << "report; use after free; " << 1 << std::endl;
            }
#endif

            return static_cast<T *>(const_cast<void *>(pi_));
        }

#if 1
        operator T const * () const
        {
#ifdef BOOST_REPORT
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            if (base::get() && base::get()->explicit_delete_ == true)
            {
                std::cerr << "report; use after free; " << 1 << std::endl;
            }
#endif

            return static_cast<T const *>(pi_);
        }
#endif
#endif

        root_ptr & operator ++ ()
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            return ++ static_cast<T * &>(pi_), * this;
        }

        root_ptr & operator -- ()
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            return -- static_cast<T * &>(pi_), * this;
        }

        root_ptr operator ++ (int)
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            root_ptr temp(* this);

            return ++ static_cast<T * &>(pi_), temp;
        }

        root_ptr operator -- (int)
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            root_ptr temp(* this);

            return -- static_cast<T * &>(pi_), temp;
        }

        ptrdiff_t operator - (root_ptr const & o) const
        {
            return static_cast<T * &>(pi_) - static_cast<T * &>(o.pi_);
        }

#if 1
        template <typename V>
            root_ptr operator + (V i) const
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                root_ptr res(* this);
                
                res.pi_ = static_cast<T const *>(res.pi_) + i;
                
                return res;
            }

        template <typename V>
            root_ptr operator - (V i) const
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                root_ptr res(* this);
                
                res.pi_ = static_cast<T const *>(res.pi_) - i;
                
                return res;
            }
#endif

        template <typename V>
            root_ptr & operator += (V i)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

#ifndef BOOST_NO_EXCEPTIONS
                if (! pi_)
                {
                    std::stringstream out;
                    out << "null pointer\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }
#endif

                pi_ = static_cast<T const *>(pi_) + i;
                
                return * this;
            }

        template <typename V>
            root_ptr & operator -= (V i)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

#ifndef BOOST_NO_EXCEPTIONS
                if (! pi_)
                {
                    std::stringstream out;
                    out << "null pointer\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }
#endif

                pi_ = static_cast<T const *>(pi_) - i;

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

        ~root_ptr()
        {
#ifdef BOOST_REPORT
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            if (base::get() && base::get()->explicit_delete_ == false)
            {
                std::cerr << "report; memory leak; " << base::get()->size_bytes() << std::endl;
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
    class root_ptr<void> : protected root_core
    {
        template <typename> friend class root_ptr;

        template <typename U, typename V> friend root_ptr<U> static_pointer_cast(root_ptr<V> const & p);
        template <typename U, typename V> friend root_ptr<U> dynamic_pointer_cast(root_ptr<V> const & p);
        template <typename U, typename V> friend root_ptr<U> reinterpret_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V const> const_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V> const_pointer_cast(root_ptr<V const> const & p);

    protected:
        typedef root_core base;

    public:
        typedef typename base::value_type value_type;


        root_ptr(root_ptr const & p)
        : base(p)
        {
        }

        template <typename V>
            root_ptr(root_ptr<V> const & p)
            : base(p)
            {
            }

            root_ptr(root_ptr<std::nullptr_t> const & p)
            : base(p)
            {
            }

#if defined(BOOST_HAS_RVALUE_REFS)
        template <typename V>
            root_ptr(root_ptr<V> && p)
            : base(std::move(p))
            {
            }
#endif

        root_ptr(node_proxy const & x)
        : base(x)
        {
        }

        root_ptr(node_proxy const & x, std::uintptr_t p)
        : base(x, reinterpret_cast<void *>(p))
        {
        }

        template <typename V>
            root_ptr(node_proxy const & x, V * p)
            : base(x, p)
            {
            }

        template <typename V, typename PoolAllocator>
            root_ptr(node_proxy const & x, node<V, PoolAllocator> * p)
            : base(x, p)
            {
            }

            root_ptr(node_proxy const & x, root_ptr const & p)
            : base(p)
            {
            }
            
        template <typename V>
            root_ptr(node_proxy const & x, root_ptr<V> const & p)
            : base(p)
            {
            }
 
        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            root_ptr(root_ptr<V> const & p, static_cast_tag const & t)
            : base(p, static_cast<void *>(p.pi_))
            {
            }


        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            root_ptr(root_ptr<V> const & p, dynamic_cast_tag const & t)
            : base(p, dynamic_cast<void *>(p.pi_))
            {
            }


        root_ptr & operator = (root_ptr<std::nullptr_t> const & p)
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            return static_cast<root_ptr &>(base::operator = (p));
        }

        template <typename V, typename PoolAllocator>
            root_ptr & operator = (node<V, PoolAllocator> * p)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                return static_cast<root_ptr &>(base::template operator = <V, PoolAllocator>(p));
            }

            root_ptr & operator = (root_ptr const & p)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                return static_cast<root_ptr &>(base::operator = (p));
            }

        bool operator ! () const
        {
            return pi_ == 0;
        }

        operator void * ()
        {
            return const_cast<void *>(pi_);
        }

        operator void const * () const
        {
            return pi_;
        }

        operator uintptr_t () const
        {
#ifdef BOOST_REPORT
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            if (base::get() && base::get()->explicit_delete_ == true)
            {
                std::cerr << "report; use after free; " << 1 << std::endl;
            }
#endif

            return reinterpret_cast<uintptr_t>(pi_);
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

        ~root_ptr()
        {
#ifdef BOOST_REPORT
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            if (base::get() && base::get()->explicit_delete_ == false)
            {
                std::cerr << "report; memory leak; " << base::get()->size_bytes() << std::endl;
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

#if 1
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

                return * (static_cast<T *>(const_cast<void *>(pi_)) + n);
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

                return * (static_cast<T const *>(pi_) + n);
            }
#endif
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
    inline bool operator == (root_core const &a1, root_core const &a2)
    {
        return a1.get() == a2.get();
    }


/**
    Comparison operator.

    @param  a1  Operand 1.
    @param  a2  Operand 2.
*/

template <typename T>
    inline bool operator != (root_core const &a1, root_core const &a2)
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
