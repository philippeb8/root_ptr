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


template <typename T>
    class root_ptr;

template <>
    class root_ptr<void>;
    
template <>
    class root_ptr<std::nullptr_t>;
    
template <typename T, size_t S>
    class root_array;

    
BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION(__proxy)


template <typename T, bool = has_static_member_function___proxy<T, T const * (node_proxy &, T const *)>::value>
    struct proxy
    {
        inline T const * operator () (node_proxy * __y, T const * po, size_t s = 1) const
        {
            return po; 
        }
    };

template <>
    struct proxy<void, false>
    {
        inline void * operator () (node_proxy * __y, void * po, size_t s = 1)
        {
            return po;
        }
    };

template <typename T, size_t S>
    struct proxy<std::array<T, S>, false>
    {
        inline std::array<T, S> const * operator () (node_proxy * __y, std::array<T, S> const * po, size_t s = 1) const
        {
            for (auto pi = po; pi != po + s; ++ pi)
                for (typename std::array<T, S>::const_iterator i = pi->begin(); i != pi->end(); ++ i)
                    proxy<T>()(__y, &* i);
            
            return po;
        }
    };

template <typename T>
    struct proxy<std::vector<T>, false>
    {
        inline std::vector<T> const * operator () (node_proxy * __y, std::vector<T> const * po, size_t s = 1) const
        {
            for (auto pi = po; pi != po + s; ++ pi)
                for (typename std::vector<T>::const_iterator i = pi->begin(); i != pi->end(); ++ i)
                    proxy<T>()(__y, &* i);
            
            return po;
        }
    };

template <>
    struct proxy<std::vector<void>, false>
    {
        inline std::vector<void> const * operator () (node_proxy * __y, std::vector<void> const * po, size_t s = 1) const
        {
            return po;
        }
    };
    
template <typename T>
    struct proxy<root_ptr<T>, false>
    {
        inline root_ptr<T> const * operator () (node_proxy * x, root_ptr<T> const * po, size_t s = 1) const
        {
            for (auto pi = po; pi != po + s; ++ pi)
                pi->proxy(x);
            
            return po;
        }
    };

template <typename T, size_t S>
    struct proxy<root_array<T, S>, false>
    {
        inline root_array<T, S> const * operator () (node_proxy * x, root_array<T, S> const * po, size_t s = 1) const
        {
            for (auto pi = po; pi != po + s; ++ pi)
                pi->proxy(x);
            
            return po;
        }
    };

template <typename T>
    struct proxy<T, true>
    {
        inline T const * operator () (node_proxy * __y, T const * po, size_t s = 1) const
        { 
            for (auto pi = po; pi != po + s; ++ pi)
                T::__proxy(* __y, const_cast<T *>(pi));
            
            return po;
        }
    };

template <typename T>
    inline T & make_proxy(node_proxy * __y, T const & po)
    { 
        return const_cast<T &>(* proxy<T>()(__y, & po));
    }

    
/**
    Deterministic region based memory manager.

    Complete memory management utility on top of standard reference counting.

    @note Must be initialized with a reference to a @c node_proxy , given by a @c node_ptr<> .
*/

template <typename T>
class node_ptr : public smart_ptr::detail::node_ptr_common<T>
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
, public smart_ptr::detail::node_ptr_common<std::vector<T>>
#endif
    {
        template <typename> friend class node_ptr;

        
    protected:
        /** Reference to the @c node_proxy node @c node_ptr<> belongs to. */
        mutable node_proxy * px_;
        

    public:
        typedef typename smart_ptr::detail::node_ptr_common<T> base1;
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
        typedef typename smart_ptr::detail::node_ptr_common<std::vector<T>> base2;
#endif
        
        
        /**
            Initialization of a pointer.

            @param  x   Reference to a @c node_proxy the pointer belongs to.
        */

        explicit node_ptr(node_proxy & x)
        : base1()
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
        , base2()
#endif
        , px_(& x)
        {
        }


        /**
            Initialization of a pointer.

            @param  x   Reference to a @c node_proxy the pointer belongs to.
            @param  p New pointee object to manage.
        */

        template <typename V, typename PoolAllocator>
            explicit node_ptr(node_proxy & x, node<V, PoolAllocator> * p)
            : base1(p)
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
            , base2()
#endif
            , px_(& x)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
                
                px_->init(p);
            }


        /**
            Initialization of a pointer.

            @param  x   Reference to a @c node_proxy the pointer belongs to.
            @param  p New pointee object to manage.
        */

        template <typename V, size_t S, typename PoolAllocator>
            explicit node_ptr(node_proxy & x, node<std::array<V, S>, PoolAllocator> * p)
            : base1(p)
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
            , base2()
#endif
            , px_(& x)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
                
                px_->init(p);
            }

        /**
            Initialization of a pointer.

            @param  x   Reference to a @c node_proxy the pointer belongs to.
            @param  p New pointee object to manage.
        */

        template <typename V, typename PoolAllocator>
            explicit node_ptr(node_proxy & x, node<std::vector<V>, PoolAllocator> * p)
            : base1()
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
            , base2(p)
#endif
            , px_(& x)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
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
            node_ptr(node_ptr<V> const & p)
            : base1(p)
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
            , base2(p)
#endif
            , px_(p.px_)
            {
            }

            
#if defined(BOOST_HAS_RVALUE_REFS)
        template <typename V>
            node_ptr(node_ptr<V> && p)
            : base1(std::move(p))
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
            , base2(std::move(p))
#endif
            , px_(std::move(p.px_))
            {
            }
#endif


        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            node_ptr(node_ptr<V> const & p, smart_ptr::detail::static_cast_tag const & t)
            : base1(static_cast<smart_ptr::detail::node_ptr_common<V> const &>(p), t)
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
            , base2()
#endif
            , px_(p.px_)
            {
            }


        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            node_ptr(node_ptr<V> const & p, smart_ptr::detail::dynamic_cast_tag const & t)
            : base1(static_cast<smart_ptr::detail::node_ptr_common<V> const &>(p), t)
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
            , base2()
#endif
            , px_(p.px_)
            {
            }


        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

            node_ptr(node_ptr<T> const & p)
            : base1(p)
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
            , base2(p)
#endif
            , px_(p.px_)
            {
            }

            
        /**
            Assignment.

            @param  p   New pointer expected to be null.
        */

        node_ptr & operator = (std::nullptr_t)
        {
            base1::reset(nullptr);
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
            base2::reset(nullptr);
#endif
            
            return * this;
        }

        
#if 0
        /**
            Assignment.

            @param  p New pointer to manage.
        */

        node_ptr & operator = (node_ptr<T> const & p)
        {
            return operator = <T>(p);
        }
#endif


        /**
            Assignment.

            @param  p   New pointer to manage.
        */

        template <typename V, typename PoolAllocator>
            node_ptr & operator = (node<V, PoolAllocator> * p)
            {
                px_->init(p);

                base1::operator = (p);
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
                base2::reset(nullptr);
#endif
                
                return * this;
            }


        /**
            Assignment.

            @param  p   New pointer to manage.
        */

        template <typename V, typename PoolAllocator>
            node_ptr & operator = (node<std::vector<V>, PoolAllocator> * p)
            {
                px_->init(p);

                base1::reset(nullptr);
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
                base2::operator = (p);
#endif

                return * this;
            }


        /**
            Assignment.

            @param  p New pointer to manage.
        */

        template <typename V>
            node_ptr & operator = (node_ptr<V> const & p)
            {
                if (p.px_->depth() < px_->depth())
                    proxy(p.px_);
                else if (px_->depth() < p.px_->depth())
                    p.proxy(px_);
                
                base1::operator = (static_cast<typename node_ptr<V>::base1 const &>(p));
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
                base2::operator = (static_cast<typename node_ptr<V>::base2 const &>(p));
#endif
                
                return * this;
            }


        /**
            Assignment.

            @param  p New pointer to manage.
        */

            node_ptr & operator = (node_ptr const & p)
            {
                if (p.px_->depth() < px_->depth())
                    proxy(p.px_);
                else if (px_->depth() < p.px_->depth())
                    p.proxy(px_);
                
                base1::operator = (static_cast<node_ptr::base1 const &>(p));
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
                base2::operator = (static_cast<node_ptr::base2 const &>(p));
#endif
                
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
        
        node_proxy & proxy() 
        {
            return * px_;
        }

        
        /**
            Returns associated proxy.
            
            @return     @c node_proxy part of @c node_ptr .
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

                if (base1::po_)
                {
                    base1::header()->node_tag_.erase();
                    px_->init(base1::header());
                    boost::proxy<T>()(x, base1::po_, base1::header()->size());
                }
                
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
                if (base2::po_)
                {
                    base2::header()->node_tag_.erase();
                    px_->init(base2::header());
                    boost::proxy<std::vector<T>>()(x, base2::po_);
                }
#endif
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

        ~node_ptr()
        {
            if (cyclic())
            {
                base1::po_ = nullptr;
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
                base2::po_ = nullptr;
#endif
            }
        }
    };


template <>
    class root_ptr<std::nullptr_t> : public node_ptr<std::nullptr_t>
    {
        template <typename> friend class root_ptr;
        
        template <typename U, typename V> friend U static_pointer_cast(V const & p);
        template <typename U, typename V> friend U dynamic_pointer_cast(V const & p);
        template <typename U, typename V> friend U reinterpret_pointer_cast(V const & p);
        template <typename V> friend root_ptr<V const> const_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V> const_pointer_cast(root_ptr<V const> const & p);
        
        typedef node_ptr<std::nullptr_t> base;
        
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
            recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
                
            return os << o.operator std::nullptr_t * ();
        }

        ~root_ptr()
        {
            //if (po_ && ! cyclic() && header()->use_count() == 1)
            //    BOOSstd::nullptr_t_LOG_std::nullptr_tRIVIAL(info) << "\"" << pn_ << "\":" << boost::stacktrace::stacktrace(1, 1);
        }
    };
    
    
template <>
    class root_ptr<void> : public node_ptr<void>
    {
        template <typename> friend class root_ptr;
        
        template <typename U, typename V> friend U static_pointer_cast(V const & p);
        template <typename U, typename V> friend U dynamic_pointer_cast(V const & p);
        template <typename U, typename V> friend U reinterpret_pointer_cast(V const & p);
        template <typename V> friend root_ptr<V const> const_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V> const_pointer_cast(root_ptr<V const> const & p);
        
        typedef node_ptr<void> base;
        
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
            
#if defined(BOOST_HAS_RVALUE_REFS)
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
            , pi_(p->element())
            , pn_(n)
            {
            }

        template <typename V, typename PoolAllocator>
            root_ptr(node_proxy & x, char const * n, node<std::vector<V>, PoolAllocator> * p) 
            : base(x, p)
            , pi_(p->element()->data())
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
            root_ptr(root_ptr<V> const & p, char const * n, smart_ptr::detail::static_cast_tag const & t)
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
            root_ptr(root_ptr<V> const & p, char const * n, smart_ptr::detail::dynamic_cast_tag const & t)
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
            recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
            
            pi_ = nullptr;
            
            return static_cast<root_ptr &>(base::operator = (nullptr));
        }
        
        template <typename V>
            root_ptr & operator = (V const * p)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
                
                pi_ = const_cast<V *>(p);
                
                return * this;
            }

        template <typename V, typename PoolAllocator>
            root_ptr & operator = (node<V, PoolAllocator> * p)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
                
                pi_ = p->element();
                
                return static_cast<root_ptr &>(base::template operator = <V, PoolAllocator>(p));
            }

        template <typename V, typename PoolAllocator>
            root_ptr & operator = (node<std::vector<V>, PoolAllocator> * p)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
                
                pi_ = p->element()->data();
                
                return static_cast<root_ptr &>(base::template operator = <V, PoolAllocator>(p));
            }

            root_ptr & operator = (root_ptr const & p)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
                
                pi_ = p.pi_;
                
                return static_cast<root_ptr &>(base::template operator = <void>(p));
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
            recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
                
            return os << o.operator void * ();
        }

        ~root_ptr()
        {
            //if (po_ && ! cyclic() && header()->use_count() == 1)
            //    BOOSvoid_LOG_voidRIVIAL(info) << "\"" << pn_ << "\":" << boost::stacktrace::stacktrace(1, 1);
        }
    };


template <typename T>
    class root_ptr : public node_ptr<T>
    {
        template <typename> friend class root_ptr;
        
        template <typename U, typename V> friend U static_pointer_cast(V const & p);
        template <typename U, typename V> friend U dynamic_pointer_cast(V const & p);
        template <typename U, typename V> friend U reinterpret_pointer_cast(V const & p);
        template <typename V> friend root_ptr<V const> const_pointer_cast(root_ptr<V> const & p);
        template <typename V> friend root_ptr<V> const_pointer_cast(root_ptr<V const> const & p);
        
        typedef node_ptr<T> base;
        
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
            : base(p, smart_ptr::detail::static_cast_tag())
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
            
#if defined(BOOST_HAS_RVALUE_REFS)
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
            , pi_(static_cast<V *>(const_cast<V *>(p)))
            , pn_(n)
            {
            }
        
        template <typename V, typename PoolAllocator>
            root_ptr(node_proxy & x, char const * n, node<V, PoolAllocator> * p) 
            : base(x, p)
            , pi_(p->element())
            , pn_(n)
            {
            }

        template <typename V, size_t S, typename PoolAllocator>
            root_ptr(node_proxy & x, char const * n, node<std::array<V, S>, PoolAllocator> * p) 
            : base(x, p)
            , pi_(p->element()->data())
            , pn_(n)
            {
            }
            
        template <typename V, typename PoolAllocator>
            root_ptr(node_proxy & x, char const * n, node<std::vector<V>, PoolAllocator> * p) 
            : base(x, p)
            , pi_(p->element()->data())
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
            : base(p, smart_ptr::detail::static_cast_tag())
            , pi_(static_cast<T *>(p.pi_))
            , pn_(n)
            {
            }
        
            
        /**
            Initialization of a pointer.

            @param  p New pointer to manage.
        */

        template <typename V>
            root_ptr(root_ptr<V> const & p, char const * n, smart_ptr::detail::static_cast_tag const & t)
            : base(p, t)
            , pi_(static_cast<T *>(p.pi_))
            , pn_(n)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
                
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
                if (p.base::base2::po_)
                {
                    std::stringstream out;
                    out << "\"" << p.name() << "\" is not a single object\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }
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
            root_ptr(root_ptr<V> const & p, char const * n, smart_ptr::detail::dynamic_cast_tag const & t)
            : base(p, t)
            , pi_(dynamic_cast<T *>(p.pi_))
            , pn_(n)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
                
#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
                if (p.base::base2::po_)
                {
                    std::stringstream out;
                    out << "\"" << p.name() << "\" is not a single object\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }
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
            recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif        

            pi_ = nullptr;
            
            return static_cast<root_ptr &>(base::operator = (nullptr));
        }
        
        template <typename V>
            root_ptr & operator = (V const * p)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif        

                pi_ = const_cast<V *>(p);
                
                return * this;
            }

        template <typename V, typename PoolAllocator>
            root_ptr & operator = (node<V, PoolAllocator> * p)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif        

                pi_ = p->element();
                
                return static_cast<root_ptr &>(base::template operator = <V, PoolAllocator>(p));
            }

        template <typename V, size_t S, typename PoolAllocator>
            root_ptr & operator = (node<std::array<V, S>, PoolAllocator> * p)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif        

                pi_ = p->element()->data();
                
                return static_cast<root_ptr &>(base::template operator = <V, PoolAllocator>(p));
            }

        template <typename V, typename PoolAllocator>
            root_ptr & operator = (node<std::vector<V>, PoolAllocator> * p)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif        

                pi_ = p->element()->data();
                
                return static_cast<root_ptr &>(base::template operator = <V, PoolAllocator>(p));
            }

        template <typename V>
            root_ptr & operator = (root_ptr<V> const & p)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif        

                pi_ = p.pi_;
                
                return static_cast<root_ptr &>(base::template operator = <V>(p));
            }

            root_ptr & operator = (root_ptr const & p)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif        

                pi_ = p.pi_;
                
                return static_cast<root_ptr &>(base::operator = (p));
            }

        template <typename V>
            T & operator [] (V n)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
                
                if (base::base1::po_)
                    if (base::base1::header()->size() <= n)
                    {
                        std::stringstream out;
                        out << "\"" << name() << "\" (" << n << ") out of range [0, " << base::base1::header()->size() << "[\n";
                        node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                        throw std::out_of_range(out.str());
                    }

#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
                if (base::base2::po_)
                    if (base::base2::po_->size() <= n)
                    {
                        std::stringstream out;
                        out << "\"" << name() << "\" (" << n << ") out of range [0, " << base::base2::po_->size() << "[\n";
                        node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                        throw std::out_of_range(out.str());
                    }
#endif
                
                if (! pi_)
                {
                    std::stringstream out;
                    out << "\"" << name() << "\" is null pointer\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }            
                
                return * (pi_ + n); 
            }

        template <typename V>
            T const & operator [] (V n) const
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
            
                if (base::base1::po_)
                    if (base::base1::header()->size() <= n)
                    {
                        std::stringstream out;
                        out << "\"" << name() << "\" (" << n << ") out of range [0, " << base::base1::header()->size() << "[\n";
                        node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                        throw std::out_of_range(out.str());
                    }

#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
                if (base::base2::po_)
                    if (base::base2::po_->size() <= n)
                    {
                        std::stringstream out;
                        out << "\"" << name() << "\" (" << n << ") out of range [0, " << base::base2::po_->size() << "[\n";
                        node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                        throw std::out_of_range(out.str());
                    }
#endif
                
                if (! pi_)
                {
                    std::stringstream out;
                    out << "\"" << name() << "\" is null pointer\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }            
                
                return * (pi_ + n); 
            }

        T & operator * () const
        {
#ifndef BOOST_DISABLE_THREADS
            recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
            
            if (base::base1::po_)
                if (pi_ < base::base1::po_ || pi_ >= base::base1::po_ + base::base1::header()->size())
                {
                    std::stringstream out;
                    out << "\"" << name() << "\" (" << pi_ - base::base1::po_ << ") out of range [0, " << base::base1::header()->size() << "[\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }

#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
            if (base::base2::po_)
                if (pi_ < base::base2::po_->data() || pi_ >= base::base2::po_->data() + base::base2::po_->size())
                {
                    std::stringstream out;
                    out << "\"" << name() << "\" (" << pi_ - base::base2::po_->data() << ") out of range [0, " << base::base2::po_->size() << "[\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }
#endif
                
            if (! pi_)
            {
                std::stringstream out;
                out << "\"" << name() << "\" is null pointer\n";
                node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                throw std::out_of_range(out.str());
            }            
            
            return * pi_;
        }

        T * operator -> () const
        {
#ifndef BOOST_DISABLE_THREADS
            recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
            
            if (base::base1::po_)
                if (pi_ < base::base1::po_ || pi_ >= base::base1::po_ + base::base1::header()->size())
                {
                    std::stringstream out;
                    out << "\"" << name() << "\" (" << pi_ - base::base1::po_ << ") out of range [0, " << base::base1::header()->size() << "[\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }

#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
            if (base::base2::po_)
                if (pi_ < base::base2::po_->data() || pi_ >= base::base2::po_->data() + base::base2::po_->size())
                {
                    std::stringstream out;
                    out << "\"" << name() << "\" (" << pi_ - base::base2::po_->data() << ") out of range [0, " << base::base2::po_->size() << "[\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }
#endif

            if (! pi_)
            {
                std::stringstream out;
                out << "\"" << name() << "\" is null pointer\n";
                node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                throw std::out_of_range(out.str());
            }            
            
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

        operator T * () const
        {
            if (base::base1::po_)
                if (pi_ < base::base1::po_ || pi_ >= base::base1::po_ + base::base1::header()->size())
                {
                    std::stringstream out;
                    out << "\"" << name() << "\" (" << pi_ - base::base1::po_ << ") out of range [0, " << base::base1::header()->size() << "[\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }

#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
            if (base::base2::po_)
                if (pi_ < base::base2::po_->data() || pi_ >= base::base2::po_->data() + base::base2::po_->size())
                {
                    std::stringstream out;
                    out << "\"" << name() << "\" (" << pi_ - base::base2::po_->data() << ") out of range [0, " << base::base2::po_->size() << "[\n";
                    node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                    throw std::out_of_range(out.str());
                }
#endif

            if (! pi_)
            {
                std::stringstream out;
                out << "\"" << name() << "\" is null pointer\n";
                node_proxy::stacktrace(out, * node_proxy::top_node_proxy());
                throw std::out_of_range(out.str());
            }            
            
            return pi_;
        }

        root_ptr & operator ++ ()
        {
#ifndef BOOST_DISABLE_THREADS
            recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
            
            return ++ pi_, * this;
        }

        root_ptr & operator -- ()
        {
#ifndef BOOST_DISABLE_THREADS
            recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
            
            return -- pi_, * this;
        }
        
        root_ptr operator ++ (int)
        {
#ifndef BOOST_DISABLE_THREADS
            recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
            
            root_ptr temp(* this);
            
            return ++ pi_, temp;
        }

        root_ptr operator -- (int)
        {
#ifndef BOOST_DISABLE_THREADS
            recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
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
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
                
                pi_ += i;
                
                return * this;
            }

        template <typename V>
            root_ptr & operator -= (V i)
            {
#ifndef BOOST_DISABLE_THREADS
                recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
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
            recursive_mutex::scoped_lock scoped_lock(smart_ptr::detail::static_recursive_mutex());
#endif
                
            return os << o.operator T * ();
        }

        ~root_ptr()
        {
            //if (po_ && ! cyclic() && header()->use_count() == 1)
            //    BOOST_LOG_TRIVIAL(info) << "\"" << pn_ << "\":" << boost::stacktrace::stacktrace(1, 1);
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
            
        inline node<std::array<T, S>> * from_range(node_proxy & __y, T const * begin, T const * end) const
        {
            return new node<std::array<T, S>>(begin, end);
        }
    
        inline node<std::array<T, S>> * from_initializer(node_proxy & __y, std::initializer_list<T> && l) const
        {
            return new node<std::array<T, S>>(construct<std::array<T, S>>()(__y, "", std::forward<std::initializer_list<T>>(l)));
        }
    };

#ifndef BOOST_DISABLE_ROOT_PTR_VECTORS
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
#endif


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
        return T(p, "", smart_ptr::detail::static_cast_tag());
    }


/**
    Dynamic cast.
*/

template <typename T, typename V>
    inline T dynamic_pointer_cast(V const & p)
    {
        return T(p, "", smart_ptr::detail::dynamic_cast_tag());
    }

    
/**
    Reinterpret cast.
*/

template <typename T, typename V>
    inline T reinterpret_pointer_cast(V const & p)
    {
        return T(root_ptr<void>(p, "", smart_ptr::detail::static_cast_tag()), "", smart_ptr::detail::static_cast_tag());
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
