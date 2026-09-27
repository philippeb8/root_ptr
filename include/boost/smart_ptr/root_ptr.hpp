/**
    @file
    @brief Boost root_ptr.hpp main header file.

    Patent US11288049B2
    'SOURCE TO SOURCE COMPILER, COMPILATION METHOD, AND
    COMPUTER-READABLE MEDIUM FOR PREDICTABLE MEMORY MANAGEMENT'

    @copyright Copyright (C) 2020-2026 Services Informatiques Fornux

    @author Phil Bouchard, Founder & CEO
    Services Informatiques Fornux
    phil@fornux.com
    20 Poirier St., Gatineau, Quebec, Canada, J8V 1A6

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
#include <memory>
#include <type_traits>
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
/// Global mutex used for thread safety.
inline std::recursive_mutex & static_recursive_mutex()
{
    static std::recursive_mutex mutex_;

    return mutex_;
}
#endif


/**
    @brief Set header.

    Links a list of @c node blocks and a list of @c node_proxy.
*/

struct node_proxy
{
    /// Source file name.
    char const * file_;

    /// Function name.
    char const * function_;

    /// Line number.
    unsigned line_;

    /// Parent proxy.
    node_proxy const * parent_;

    /// Stack depth.
    size_t const depth_;

    /// Set while the proxy is being destroyed.
    bool destroying_;

    /// Pointer instances belonging to this proxy.
    mutable smart_ptr::detail::intrusive_list node_set_;

    mutable smart_ptr::detail::intrusive_list root_set_;


    /** @brief Constructs a proxy for a source location. */

    node_proxy(char const * file, char const * function, unsigned line, node_proxy const * parent = nullptr, size_t depth = 0) : file_(file), function_(function), line_(line), parent_(parent), depth_(parent ? parent->depth_ + 1 : 0), destroying_(false)
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


    /** @brief Copying is disabled. */

    node_proxy(node_proxy const & x) = delete;


    /** @brief Returns this proxy (function-style access). */

    node_proxy const & operator () () const
    {
        return * this;
    }


    /** @brief Destroys the proxy and detaches it from the other proxies. */

    ~node_proxy() noexcept(false)
    {
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            reset();

            * top_node_proxy() = parent();
        }

        // Rethrow an exception a managed destructor threw during collection, unless
        // the stack is already unwinding.
        std::exception_ptr & pending = smart_ptr::detail::pending_destructor_exception();

        if (pending)
        {
            std::exception_ptr e = pending;
            pending = nullptr;

            if (std::uncaught_exceptions() == 0)
                std::rethrow_exception(e);
        }
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


    /** @brief Releases or delegates a series of proxies. */

    void reset();
};


#ifdef BOOST_NO_EXCEPTIONS

// Inline: this header is included by every translation unit.
inline void throw_exception(std::exception const & e)
{
    std::cerr << e.what() << "\n";
    node_proxy::stacktrace(std::cerr, * node_proxy::top_node_proxy());

    exit(-1);
}

#endif


struct static_cast_tag {};

/** @brief Casts @p p to @c T*, removing the cv-qualifiers for the intermediate @c void* step. */
template <typename T, typename V>
    inline T * fcxxss_pointer_cast_cv(V * p)
    {
        return static_cast<T *>(static_cast<void *>(const_cast<typename std::remove_cv<V>::type *>(p)));
    }
struct dynamic_cast_tag {};



/**
    @brief Storage of the default instance of @c T.

    A null dereference or an out-of-bounds subscript returns a reference to
    this object instead of throwing or reading past the end, so legacy code
    keeps running. The storage is zero-initialized and no constructor runs,
    so @c T may be abstract or lack a default constructor.
*/

template <typename T, typename = void>
    struct default_storage
    {
        // T is incomplete (an opaque handle): there is no storage to hand out.
        static T & get()
        {
            return * static_cast<T *>(nullptr);
        }
    };

template <typename T>
    struct default_storage<T, decltype(void(sizeof(T)))>
    {
        static T & get()
        {
            static typename std::aligned_storage<sizeof(T), alignof(T)>::type storage{};

            return * reinterpret_cast<T *>(& storage);
        }
    };

template <typename T>
    inline T & default_instance()
    {
        return default_storage<T>::get();
    }


struct root_core
{
    typedef node_base value_type;

    /// Links this pointer into the root set of its @c node_proxy.
    mutable smart_ptr::detail::intrusive_list root_tag_;

    value_type * po_;


    explicit root_core(node_proxy const & x)
    : po_(nullptr)
    {
        x.root_set_.push_back(& root_tag_);
    }

    template <typename V, typename PoolAllocator>
        explicit root_core(node_proxy const & x, node<V, PoolAllocator> * p)
        : po_(p)
        {
            using namespace smart_ptr::detail;
            
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            x.root_set_.push_back(& root_tag_);
        }

    /**
        @brief Copy constructor.

        @param p Pointer to share.
    */

    root_core(root_core const & p)
    : po_(p.share())
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

        // Unlink under the lock, so that the member destructor touches no shared list.
        root_tag_.erase();
    }

#if defined(BOOST_HAS_RVALUE_REFS)
    root_core(root_core && p)
    : po_(std::exchange(p.po_, nullptr))
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

            return * this;
        }


    /**
        @brief Assignment.

        @param p New pointer to manage.
    */

    template <typename V>
        root_core & operator = (root_core const & p)
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            root_tag_.push_back(& p.root_tag_);

            reset(p.share());

            return * this;
        }


    /**
        @brief Assignment.

        @param p New pointer to manage.
    */

    root_core & operator = (root_core const & p)
    {
#ifndef BOOST_DISABLE_THREADS
        std::scoped_lock guard(static_recursive_mutex());
#endif

        root_tag_.push_back(& p.root_tag_);

        reset(p.share());

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

    /**
        @brief Manages @p p instead, releasing the previous node.

        The new pointer is stored first: in a cycle, releasing the previous node
        can free the node that contains this very pointer.
    */
    void reset(value_type * p = nullptr)
    {
        value_type * old = std::exchange(po_, p);

        if (old)
        {
            old->release();
        }
    }
};


inline void node_proxy::reset()
{
    using namespace smart_ptr::detail;

    // Erase each root before resetting it: a reset may unlink other roots from this set.
    if (! destroying())
    {
        destroying(true);

        while (! root_set_.empty())
        {
            intrusive_list::iterator<root_core, & root_core::root_tag_> m = root_set_.begin();
            m->root_tag_.erase();
            m->reset();
        }

        destroying(false);
    }
}


namespace smart_ptr
{
namespace detail
{

/**
    @brief Whether @c U is a complete type at this point.

    Only used to decide whether the pointee compatibility check can reach a verdict.
*/

template <typename U, typename = void>
    struct is_complete : std::false_type
    {
    };

template <typename U>
    struct is_complete<U, decltype(void(sizeof(U)))> : std::true_type
    {
    };


/**
    @brief Whether both @c V and @c T are complete at this point.

    Keyed on the pair: a per-type answer is fixed at its first use, so one
    early use of an incomplete type would loosen every later check.
*/

template <typename V, typename T, typename = void>
    struct both_complete : std::false_type
    {
    };

template <typename V, typename T>
    struct both_complete<V, T, decltype(void(sizeof(V) + sizeof(T)))>
    : std::true_type
    {
    };

/**
    @brief Whether a @c root_ptr<V> may convert implicitly to a @c root_ptr<T>.

    Mirrors raw pointers: same pointee up to cv-qualification, related
    classes, or @c void. Two class pointees are also accepted while either
    is incomplete, since the transformer uses a type long before its
    definition.
*/
template <typename V, typename T>
    struct pointee_convertible
    : std::integral_constant<bool,
          std::is_void<V>::value
       || std::is_void<T>::value
       || std::is_same<typename std::remove_cv<V>::type,
                       typename std::remove_cv<T>::type>::value
       || (   std::is_class<V>::value
           && std::is_class<T>::value
           && (   ! both_complete<V, T>::value
               || std::is_convertible<V *, T *>::value))>
    {
    };

} // namespace detail
} // namespace smart_ptr


/** @brief Null pointer constant as a @c root_ptr. */
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

    protected:
        /// Managed raw pointer.
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

        /**
            @brief Converts to any @c V constructible from a null pointer constant.

            Constrained, so the conversion drops out instead of failing for types
            that cannot be built from @c nullptr.
        */
        template <typename V,
                  typename = typename std::enable_if<
                      std::is_constructible<V, std::nullptr_t>::value>::type>
            operator V () const
            {
                // pi_ is always null: pass a null pointer constant, not a pointer to one.
                return V(nullptr);
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


/// Tag selecting the proxy-rebinding constructor used by @c operator&.
struct proxy_rebind_tag {};

/**
    @brief Smart pointer optimized for speed and memory usage.

    This class represents a basic smart pointer interface.
*/
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

    protected:
        /// Managed raw pointer.
        T * pi_;

    public:
        typedef typename base::value_type value_type;


        root_ptr(root_ptr const & p)
        : base(p)
        , pi_(p.pi_)
        {
        }

        /** @brief Converts from another pointee type; see @c pointee_convertible. */

        template <typename V, typename = typename std::enable_if<smart_ptr::detail::pointee_convertible<V, T>::value>::type>
            root_ptr(root_ptr<V> const & p)
            : base(p)
            , pi_(fcxxss_pointer_cast_cv<T>(p.pi_))
            {
            }

        template <typename V, typename... Args>
            root_ptr(root_ptr<V (Args...)> const & p)
            : base(p)
            , pi_(fcxxss_pointer_cast_cv<T>(p.pi_))
            {
            }

            root_ptr(root_ptr<std::nullptr_t> const & p)
            : base(p)
            , pi_(nullptr)
            {
            }

#if defined(BOOST_HAS_RVALUE_REFS)
        template <typename V, typename = typename std::enable_if<smart_ptr::detail::pointee_convertible<V, T>::value>::type>
            root_ptr(root_ptr<V> && p)
            : base(std::move(p))
            , pi_(fcxxss_pointer_cast_cv<T>(std::exchange(p.pi_, nullptr)))
            {
            }
#endif

        root_ptr(node_proxy const & x)
        : base(x)
        , pi_(nullptr)
        {
        }

    protected:
        /**
            @brief Null pointer with the same proxy binding as @p x.

            Used by @c operator& to root its result under this pointer's proxy. The
            tag keeps the constructor out of implicit conversions.
        */
        root_ptr(base const & x, proxy_rebind_tag)
        : base(x)
        , pi_(nullptr)
        {
        }
    public:

#if 0
        template <size_t N>
            root_ptr(node_proxy const & x, T (& p)[N])
            : base(x)
            , pi_(p)
            {
            }
#endif

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

#if 1
        template <typename V>
            root_ptr(node_proxy const & x, V * p)
            : base(x)
            , pi_(fcxxss_pointer_cast_cv<T>(p))
            {
            }

        template <typename V, typename... Args>
            root_ptr(node_proxy const & x, V (* p)(Args...))
            : base(x)
            , pi_(fcxxss_pointer_cast_cv<T>(p))
            {
            }

        template <typename V>
            root_ptr(node_proxy const & x, V const * p)
            : base(x)
            , pi_(fcxxss_pointer_cast_cv<T>(p))
            {
            }

        template <typename V, typename... Args>
            root_ptr(node_proxy const & x, V const (* p)(Args...))
            : base(x)
            , pi_(fcxxss_pointer_cast_cv<T>(p))
            {
            }
#endif

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

        template <typename V, typename = typename std::enable_if<smart_ptr::detail::pointee_convertible<V, T>::value>::type>
            root_ptr(node_proxy const & x, root_ptr<V> const & p)
            : base(p)
            , pi_(fcxxss_pointer_cast_cv<T>(p.pi_))
            {
            }

        /**
            @brief Static cast from another pointer.

            @param p Pointer to convert.
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
            @brief Dynamic cast from another pointer.

            @param p Pointer to convert.
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

        /**
            @brief Assigns a null pointer.

            Needed for a forwarded @c std::nullptr_t, e.g. through @c std::exchange.
            There is no constructor from @c std::nullptr_t: it would be ambiguous
            with @c root_ptr(node_proxy const &, T *) for a literal @c nullptr.
        */

        root_ptr & operator = (std::nullptr_t)
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            pi_ = nullptr;

            return * this;
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

            // A null dereference yields the default instance: never a throw or a wild read.
            if (! pi_)
                return default_instance<T>();

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

            // A null dereference yields the default instance: never a throw or a wild read.
            if (! pi_)
                return & default_instance<T>();

            return pi_;
        }

        root_ptr<root_ptr<T>> operator & ()
        {
            root_ptr<root_ptr<T>> res(* this, proxy_rebind_tag());
            
            res.pi_ = this;
                
            return res;
        }

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

#if 1
        /**
            @brief Converts to the raw pointer.

            A const @c root_ptr is a const pointer, not a pointer to const, so the
            result is @c T* like @c operator* and @c operator->. Use
            @c root_ptr<const T> for a pointer to const.
        */

        operator T * () const
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

        ptrdiff_t operator - (root_ptr const & o) const
        {
            return pi_ - o.pi_;
        }

#if 1
        /**
            @brief Pointer arithmetic.

            Constrained to an integral operand, so that @c p @c - @c q selects the
            pointer difference.
        */

        template <typename V, typename = typename std::enable_if<std::is_integral<V>::value>::type>
            root_ptr operator + (V i) const
            {
                root_ptr res(* this);
                
                res.pi_ += i;
                
                return res;
            }

        template <typename V, typename = typename std::enable_if<std::is_integral<V>::value>::type>
            root_ptr operator - (V i) const
            {
                root_ptr res(* this);
                
                res.pi_ -= i;
                
                return res;
            }
#endif

        template <typename V, typename = typename std::enable_if<std::is_integral<V>::value>::type>
            root_ptr & operator += (V i)
            {
#ifndef BOOST_DISABLE_THREADS
                std::scoped_lock guard(static_recursive_mutex());
#endif

                pi_ += i;

                return * this;
            }

        template <typename V, typename = typename std::enable_if<std::is_integral<V>::value>::type>
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
/** @brief @c root_ptr to a const pointee. */
template <typename T>
    class root_ptr<const T> : public root_ptr<T>
    {
    public:
        using root_ptr<T>::root_ptr;
        
        root_ptr(root_ptr<T> const & p)
        : root_ptr<T>(p)
        {
        }

        /**
            @brief Assigns a null pointer.

            Declared here because this specialization's implicit copy assignment
            hides the base's @c operator=(std::nullptr_t).
        */
        root_ptr & operator = (std::nullptr_t)
        {
            root_ptr<T>::operator = (nullptr);
            return * this;
        }


        root_ptr<root_ptr> operator & ()
        {
            root_ptr<root_ptr> res(* this, proxy_rebind_tag());
                
            res.pi_ = this;
                
            return res;
        }

        /**
            @brief Pointer arithmetic that keeps the pointee const.

            The inherited operators would return @c root_ptr<T>.
        */
        template <typename V, typename = typename std::enable_if<std::is_integral<V>::value>::type>
            root_ptr operator + (V i) const
            {
                return root_ptr(root_ptr<T>::operator + (i));
            }

        template <typename V, typename = typename std::enable_if<std::is_integral<V>::value>::type>
            root_ptr operator - (V i) const
            {
                return root_ptr(root_ptr<T>::operator - (i));
            }
    };
#endif


/** @brief @c root_ptr to @c void. */
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

    protected:
        /// Managed raw pointer.
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
            : base(p)
            , pi_(nullptr)
            {
            }

#if defined(BOOST_HAS_RVALUE_REFS)
        template <typename V>
            root_ptr(root_ptr<V> && p)
            : base(std::move(p))
            , pi_(std::exchange(p.pi_, nullptr))
            {
            }
#endif

        root_ptr(node_proxy const & x)
        : base(x)
        , pi_(nullptr)
        {
        }

    protected:
        /**
            @brief Null pointer with the same proxy binding as @p x.

            Used by @c operator& to root its result under this pointer's proxy. The
            tag keeps the constructor out of implicit conversions.
        */
        root_ptr(base const & x, proxy_rebind_tag)
        : base(x)
        , pi_(nullptr)
        {
        }
    public:

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
            @brief Static cast from another pointer.

            @param p Pointer to convert.
        */

        template <typename V>
            root_ptr(root_ptr<V> const & p, static_cast_tag const & t)
            : base(p)
            , pi_(static_cast<void *>(p.pi_))
            {
            }


        /**
            @brief Dynamic cast from another pointer.

            @param p Pointer to convert.
        */

        template <typename V>
            root_ptr(root_ptr<V> const & p, dynamic_cast_tag const & t)
            : base(p)
            , pi_(dynamic_cast<void *>(p.pi_))
            {
            }


        /**
            @brief Assigns a null pointer.

            Needed for a forwarded @c std::nullptr_t, e.g. through @c std::exchange.
            There is no constructor from @c std::nullptr_t: it would be ambiguous
            with @c root_ptr(node_proxy const &, T *) for a literal @c nullptr.
        */

        root_ptr & operator = (std::nullptr_t)
        {
#ifndef BOOST_DISABLE_THREADS
            std::scoped_lock guard(static_recursive_mutex());
#endif

            pi_ = nullptr;

            return * this;
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

        root_ptr<root_ptr<void>> operator & ()
        {
            root_ptr<root_ptr<void>> res(* this, proxy_rebind_tag());
                
            res.pi_ = this;
                
            return res;
        }

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

        /**
            @brief Converts to @c void*.

            A const @c root_ptr<void> is a @c void* @c const, as for the primary
            template. The @c void @c const* conversion remains the exact match for a
            @c void @c const* target.
        */
        operator void * () const
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
/** @brief @c root_ptr to @c const @c void. */
template <>
    class root_ptr<const void> : public root_ptr<void>
    {
    public:
        using root_ptr<void>::root_ptr;
        
        root_ptr(root_ptr<void> const & p)
        : root_ptr<void>(p)
        {
        }

        /**
            @brief Assigns a null pointer.

            Declared here because this specialization's implicit copy assignment
            hides the base's @c operator=(std::nullptr_t).
        */
        root_ptr & operator = (std::nullptr_t)
        {
            root_ptr<void>::operator = (nullptr);
            return * this;
        }


        root_ptr<root_ptr> operator & ()
        {
            root_ptr<root_ptr<const void>> res(* this, proxy_rebind_tag());
                
            res.pi_ = this;
                
            return res;
        }
    };
#endif


/** @brief Fixed-size array of @c S elements of @c T, managed like a @c root_ptr. */


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

        root_ptr<root_array> operator & ()
        {
            root_ptr<root_array> res(* this, proxy_rebind_tag());
                
            res.pi_ = this;
                
            return res;
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

                // Out of range yields the default instance instead of undefined behaviour.
                if (S <= n)
                    return default_instance<T>();


                return * (pi_ + n);
            }

        /** @brief Element access; the constness belongs to the array handle, not to the elements. */
        template <typename V>
            T & operator [] (V const n) const
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

                // Out of range yields the default instance instead of undefined behaviour.
                if (S <= n)
                    return default_instance<T>();


                return * (pi_ + n);
            }
#endif

        /**
            @brief Iterator to the first element.

            The @c T(&)[N] overloads of @c std::begin and @c std::end no longer
            match a converted array; these members replace them and make range-for
            work.
        */

        root_ptr<T> begin()
        {
            return static_cast<base &>(* this);
        }

        root_ptr<T> end()
        {
            return static_cast<base &>(* this) + S;
        }

        root_ptr<T> begin() const
        {
            return static_cast<base const &>(* this);
        }

        root_ptr<T> end() const
        {
            return static_cast<base const &>(* this) + S;
        }

        size_t size() const
        {
            return S;
        }
    };


/** @brief Size of @c T, in bytes. */

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
    @brief Whether @c V is one of the smart pointer types.

    Every @c root_ptr specialization derives from @c root_core and
    @c root_array derives from @c root_ptr, so one @c std::is_base_of covers
    them all.
*/

template <typename V>
    struct is_smart_pointer
        : std::is_base_of<root_core, typename std::remove_cv<typename std::remove_reference<V>::type>::type>
    {
    };


/**
    @brief Rejects a raw operand of the @c *_pointer_cast helpers at the call site.

    Converting a raw pointer to a @c root_ptr needs the caller's
    @c node_proxy, which a free function does not have.
*/

#define BOOST_ROOT_PTR_ASSERT_SMART_OPERAND(helper, keyword)                   \
    static_assert(is_smart_pointer<V>::value,                                  \
        "boost::" helper " requires a root_ptr operand. Expressions that keep " \
        "a RAW pointer - 'this', an address-of, an integer, and pointer "      \
        "arithmetic on any of those - must be cast plainly and wrapped "       \
        "instead: root_ptr<T>{__y(), " keyword "<T *>(expr)}.")


/** @brief Static cast. */

template <typename T, typename V>
    inline T static_pointer_cast(V const & p)
    {
        BOOST_ROOT_PTR_ASSERT_SMART_OPERAND("static_pointer_cast", "static_cast");
        return T(p, static_cast_tag());
    }


/**
    @brief @c static_cast to a template parameter @c T.

    Takes the pointer cast when @c T is a @c root_ptr, and a plain
    @c static_cast otherwise.
*/

template <typename T> struct is_root_ptr : std::false_type {};
template <typename T> struct is_root_ptr<root_ptr<T>> : std::true_type {};
template <typename T> struct is_root_ptr<root_ptr<T> const> : std::true_type {};
template <typename T> struct is_root_ptr<root_ptr<T> volatile> : std::true_type {};
template <typename T> struct is_root_ptr<root_ptr<T> const volatile> : std::true_type {};

/// Pointee type of a @c root_ptr type.
template <typename P> struct root_ptr_element;
template <typename T> struct root_ptr_element<root_ptr<T>> { typedef T type; };
template <typename T> struct root_ptr_element<root_ptr<T> const> { typedef T type; };

template <typename T, typename V>
    inline typename std::enable_if<is_root_ptr<typename std::remove_reference<T>::type>::value, T>::type static_cast_(V const & p)
    {
        // Built directly: the friend static_pointer_cast would yield root_ptr<root_ptr<U>>.
        return typename std::remove_cv<typename std::remove_reference<T>::type>::type(p, static_cast_tag());
    }

template <typename T, typename V>
    inline typename std::enable_if<! is_root_ptr<typename std::remove_reference<T>::type>::value, T>::type static_cast_(V && p)
    {
        return static_cast<T>(std::forward<V>(p));
    }


/**
    @brief @c static_cast of an operand that may be a raw pointer or a @c root_ptr.

    The proxy roots a raw operand.
*/

template <typename T, typename V>
    inline typename std::enable_if<is_root_ptr<typename std::decay<V>::type>::value, T>::type static_cast_(node_proxy const &, V const & p)
    {
        return T(p, static_cast_tag());
    }

template <typename T, typename V>
    inline typename std::enable_if<! is_root_ptr<typename std::decay<V>::type>::value && std::is_pointer<typename std::decay<V>::type>::value, T>::type static_cast_(node_proxy const & y, V p)
    {
        return T(y, static_cast<typename root_ptr_element<T>::type *>(p));
    }

template <typename T, typename V>
    inline typename std::enable_if<is_root_ptr<typename std::decay<V>::type>::value, T>::type reinterpret_cast_(node_proxy const &, V const & p)
    {
        return T(root_ptr<void>(p, static_cast_tag()), static_cast_tag());
    }

template <typename T, typename V>
    inline typename std::enable_if<! is_root_ptr<typename std::decay<V>::type>::value, T>::type reinterpret_cast_(node_proxy const & y, V p)
    {
        return T(y, reinterpret_cast<typename root_ptr_element<T>::type *>(p));
    }


/**
    @brief Assignment whose form is only known at instantiation.

    Calls @c __operator_assign when the left operand takes the proxy, and the
    plain assignment otherwise.
*/

template <typename L, typename R, typename = void>
    struct has_proxy_assign : std::false_type {};

template <typename L, typename R>
    struct has_proxy_assign<L, R, std::void_t<decltype(std::declval<L &>().__operator_assign(std::declval<node_proxy const &>(), std::declval<R>()))>> : std::true_type {};

template <typename L, typename R>
    inline L & assign(node_proxy const & y, L & l, R && r)
    {
        if constexpr (has_proxy_assign<L, R &&>::value)
            l.__operator_assign(y, std::forward<R>(r));
        else if constexpr (std::is_assignable<L &, R &&>::value)
            l = std::forward<R>(r);
        else
            // Conversion through a constructor that now takes the proxy: build it explicitly.
            l = L(y, std::forward<R>(r));
        return l;
    }


/** @brief Dynamic cast. */

template <typename T, typename V>
    inline T dynamic_pointer_cast(V const & p)
    {
        BOOST_ROOT_PTR_ASSERT_SMART_OPERAND("dynamic_pointer_cast", "dynamic_cast");
        return T(p, dynamic_cast_tag());
    }


/** @brief Reinterpret cast. */

template <typename T, typename V>
    inline T reinterpret_pointer_cast(V const & p)
    {
        BOOST_ROOT_PTR_ASSERT_SMART_OPERAND("reinterpret_pointer_cast", "reinterpret_cast");
        return T(root_ptr<void>(p, static_cast_tag()), static_cast_tag());
    }


/** @brief Const cast. */

template <typename T, typename V>
    inline T const_pointer_cast(V const & p)
    {
        return T(p);
    }


/**
    @brief Equality.

    @param a1 First operand.
    @param a2 Second operand.
*/

template <typename T>
    inline bool operator == (root_core const &a1, root_core const &a2)
    {
        return a1.get() == a2.get();
    }


/**
    @brief Inequality.

    @param a1 First operand.
    @param a2 Second operand.
*/

template <typename T>
    inline bool operator != (root_core const &a1, root_core const &a2)
    {
        return a1.get() != a2.get();
    }


/** @brief Generic get_value(). */

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



/**
    @brief Calls @p f with the proxy when it accepts one, and without it otherwise.

    Serves templates whose instantiations disagree about the proxy.
*/

template <typename F, typename... A>
    inline decltype(auto) proxy_call(node_proxy const & y, F && f, A &&... a)
    {
        if constexpr (std::is_invocable_v<F, node_proxy const &, A &&...>)
            return f(y, std::forward<A>(a)...);
        else
            return f(std::forward<A>(a)...);
    }

/** @brief Pointer arithmetic with the integer on the left (@c i @c + @c p). */

template <typename T, typename V,
          typename = typename std::enable_if<std::is_integral<V>::value>::type>
    inline root_ptr<T> operator + (V i, root_ptr<T> const & p)
    {
        return p + i;
    }

/**
    @brief Type-erased destroy and copy functions for generated union storage.

    The enclosing class keeps one pointer to each for the live member.
    @c ::new is used because every transformed class hides the global
    placement form.
*/

template <typename T>
    inline void union_destroy_field(T * p);
template <typename T>
    inline void union_copy_field(T * d, T const & s);

// Array members are destroyed and copied element by element.
template <typename T>
    inline void union_destroy(void * p)
    {
        union_destroy_field(static_cast<T *>(p));
    }

template <typename T>
    inline void union_copy(void * d, void const * s)
    {
        union_copy_field(static_cast<T *>(d), *static_cast<T const *>(s));
    }

/**
    @brief Per-field primitives for union members.

    A union member that is an anonymous struct has fields at several
    offsets, so each member gets its own functions that visit every field.
*/

template <typename T>
    inline void union_destroy_field(T * p)
    {
        if constexpr (std::is_array_v<T>)
        {
            for (auto & e : *p) union_destroy_field(std::addressof(e));
        }
        else
            p->~T();
    }

template <typename T>
    inline void union_copy_field(T * d, T const & s)
    {
        if constexpr (std::is_array_v<T>)
        {
            for (std::size_t i = 0; i < std::extent_v<T>; ++i)
                union_copy_field(std::addressof((*d)[i]), s[i]);
        }
        else
            ::new (static_cast<void *>(d)) T(s);
    }

/**
    @brief Value-initializes one field with the caller's proxy.

    A field that can be built neither with nor without the proxy is left alone.
*/
template <typename T>
    inline void union_value_init(T * p, node_proxy const & y)
    {
        if constexpr (std::is_array_v<T>)
        {
            for (auto & e : *p) union_value_init(std::addressof(e), y);
        }
        else if constexpr (std::is_constructible_v<T, node_proxy const &>)
            ::new (static_cast<void *>(p)) T(y);
        else if constexpr (std::is_default_constructible_v<T>)
            ::new (static_cast<void *>(p)) T();
    }

/**
    @brief Makes one union member live before a field inside it is written.

    A live member is left untouched; otherwise the live member is destroyed
    and every field of this one is built.
*/
inline void union_make_live(void * storage, void (*& dtor)(void *),
                            void (*& copier)(void *, void const *),
                            void (* d)(void *), void (* c)(void *, void const *),
                            void (* init)(void *, node_proxy const &), node_proxy const & y)
{
    if (dtor == d) return;
    if (dtor) dtor(storage);
    init(storage, y);
    dtor = d;
    copier = c;
}

/**
    @brief Stores a value into a member of generated union storage.

    Evaluates the value first, destroys the live member, move-constructs
    this one from the value and records its destroy and copy functions.
*/

template <typename T, typename V>
    inline T & union_store(T * p, void * storage, void (*& dtor)(void *),
                           void (*& copier)(void *, void const *), V && v)
    {
        T t(std::forward<V>(v));
        if (dtor) dtor(storage);
        ::new (static_cast<void *>(p)) T(std::move(t));
        dtor = & union_destroy<T>;
        copier = & union_copy<T>;
        return *p;
    }

/**
    @brief Converts an operand of a dependent operator to @c To when possible.

    Returns the operand unchanged when @c To is not constructible from it.
*/

template <typename To, typename X>
    inline decltype(auto) operand(node_proxy const & y, X && x)
    {
        using T = typename std::remove_cv<To>::type;
        if constexpr (std::is_same_v<typename std::decay<X>::type, T>)
            return std::forward<X>(x);
        else if constexpr (std::is_constructible_v<T, node_proxy const &, X &&>)
            return T(y, std::forward<X>(x));
        else if constexpr (std::is_constructible_v<T, X &&>)
            return T(std::forward<X>(x));
        else
            return std::forward<X>(x);
    }

/**
    @brief Result type of the call made by @c proxy_call.

    Like @c proxy_call, it invokes with the proxy first when possible;
    @c std::invoke_result alone reports transformed callables as not
    invocable.
*/

template <typename F, typename... A>
    struct proxy_invoke_result
    {
        typedef typename std::conditional<
            std::is_invocable_v<F, node_proxy const &, A...>,
            std::invoke_result<F, node_proxy const &, A...>,
            std::invoke_result<F, A...>>::type::type type;
    };

template <typename F, typename... A>
    using proxy_invoke_result_t = typename proxy_invoke_result<F, A...>::type;

/**
    @brief Wraps a raw pointer for a constructor parameter that became a @c root_ptr.

    Used by @c make only after the unwrapped forms have been ruled out.
*/

template <typename Y>
    inline root_ptr<Y> proxy_wrap(node_proxy const & y, Y * p)
    {
        return root_ptr<Y>(y, p);
    }

template <typename A>
    inline A && proxy_wrap(node_proxy const &, A && a)
    {
        return static_cast<A &&>(a);
    }

/**
    @brief Wraps @c nullptr for a converted pointer parameter.

    Non-template, so it beats the forwarding overload for a @c std::nullptr_t.
*/

    inline root_ptr<std::nullptr_t> proxy_wrap(node_proxy const & y, std::nullptr_t)
    {
        return root_ptr<std::nullptr_t>(y, nullptr);
    }

/**
    @brief @c std::make_unique for a transformed type.

    The transformer rewrites @c std::make_unique and @c std::make_shared
    calls to these, which pass the proxy when @c T takes it. The allocation
    happens in this header, so the smart pointer owns a plain @c new.
*/

template <typename T, typename... A>
    inline std::unique_ptr<T> proxy_make_unique(node_proxy const & y, A &&... a)
    {
        if constexpr (std::is_constructible_v<T, node_proxy const &, A &&...>)
            return std::unique_ptr<T>(new T(y, std::forward<A>(a)...));
        else
            return std::unique_ptr<T>(new T(std::forward<A>(a)...));
    }

template <typename T, typename... A>
    inline std::shared_ptr<T> proxy_make_shared(node_proxy const & y, A &&... a)
    {
        if constexpr (std::is_constructible_v<T, node_proxy const &, A &&...>)
            return std::make_shared<T>(y, std::forward<A>(a)...);
        else
            return std::make_shared<T>(std::forward<A>(a)...);
    }

/**
    @brief Compile-time error for @c new @c T handed to a system smart pointer.

    Such a pointer would later @c delete an address inside a @c node. Use
    @c std::make_unique or @c WTF::makeUnique instead. Marked unavailable
    rather than using @c static_assert, which is disabled by default.
*/

template <typename T, typename... A>
    root_ptr<T> new_into_system_owner_use_make_unique(node_proxy const &, A &&...)
        __attribute__((unavailable(
            "'new T(args)' handed to a system owning smart pointer would be "
            "freed with a plain 'delete' on a boost::node interior pointer. "
            "Write std::make_unique<T>(args...) instead (or WTF::makeUnique<T>) "
            "- there the allocation happens inside a header the transformer "
            "leaves alone, so the smart pointer owns a plain 'new'")));

/**
    @brief One proxy-aware user conversion for a single constructor argument.

    The proxy turns a one-argument converting constructor into a
    two-argument one, so implicit conversions through it are lost. This
    adaptor restores one conversion per argument, chosen by overload
    resolution against the target's constructors. It is the last resort of
    @c make and @c make_at.
*/

template <typename A>
    struct proxy_convert_arg
    {
        node_proxy const & y_;
        A && a_;

        template <typename U,
            typename = typename std::enable_if<
                ! std::is_same<typename std::decay<U>::type, node_proxy>::value
                && (std::is_constructible<typename std::decay<U>::type, node_proxy const &, A &&>::value
                    || std::is_constructible<typename std::decay<U>::type, A &&>::value)>::type>
            operator U () const
            {
                typedef typename std::decay<U>::type V;

                if constexpr (std::is_constructible_v<V, node_proxy const &, A &&>)
                    return V(y_, std::forward<A>(a_));
                else
                    return V(std::forward<A>(a_));
            }
    };

/**
    @brief Constructs a @c T whose constructor may take the @c node_proxy.

    Passes the proxy only when @c T is constructible with it, then falls back
    to the plain, wrapped and converted argument forms.
*/
template <typename T>
    struct make
    {
        template <typename... A>
            T operator () (node_proxy const & __y, A &&... a) const
            {
                if constexpr (std::is_constructible_v<T, node_proxy const &, A &&...>)
                    return T(__y, std::forward<A>(a)...);
                else if constexpr (std::is_constructible_v<T, A &&...>)
                    return T(std::forward<A>(a)...);
                else if constexpr (std::is_constructible_v<T, node_proxy const &,
                                       decltype(proxy_wrap(__y, std::forward<A>(a)))...>)
                    return T(__y, proxy_wrap(__y, std::forward<A>(a))...);
                else if constexpr (std::is_constructible_v<T, node_proxy const &,
                                       proxy_convert_arg<A>...>)
                    return T(__y, proxy_convert_arg<A>{__y, std::forward<A>(a)}...);
                else if constexpr (std::is_constructible_v<T, proxy_convert_arg<A>...>)
                    return T(proxy_convert_arg<A>{__y, std::forward<A>(a)}...);
                else
                    return T(proxy_wrap(__y, std::forward<A>(a))...);
            }
    };


/**
    @brief Whether @c ::new @c (p) @c T(A...) is well-formed.

    Unlike @c std::is_constructible, it does not require a usable destructor.
*/
template <typename T, typename... A>
    struct is_placement_constructible
    {
        template <typename U>
            static auto probe(int)
                -> decltype(::new ((void *) 0) U(std::declval<A>()...), std::true_type());
        template <typename>
            static std::false_type probe(...);

        static constexpr bool value = decltype(probe<T>(0))::value;
    };

/**
    @brief In-place counterpart of @c make.

    Constructs at @p p with the same ladder as @c make and returns the
    pointer; this also works for a class whose destructor is deleted.
*/
template <typename T>
    struct make_at
    {
        template <typename... A>
            T * operator () (void * p, node_proxy const & __y, A &&... a) const
            {
                if constexpr (is_placement_constructible<T, node_proxy const &, A &&...>::value)
                    return ::new (p) T(__y, std::forward<A>(a)...);
                else if constexpr (is_placement_constructible<T, A &&...>::value)
                    return ::new (p) T(std::forward<A>(a)...);
                else if constexpr (is_placement_constructible<T, node_proxy const &,
                                       decltype(proxy_wrap(__y, std::forward<A>(a)))...>::value)
                    return ::new (p) T(__y, proxy_wrap(__y, std::forward<A>(a))...);
                else if constexpr (is_placement_constructible<T, node_proxy const &,
                                       proxy_convert_arg<A>...>::value)
                    return ::new (p) T(__y, proxy_convert_arg<A>{__y, std::forward<A>(a)}...);
                else if constexpr (is_placement_constructible<T, proxy_convert_arg<A>...>::value)
                    return ::new (p) T(proxy_convert_arg<A>{__y, std::forward<A>(a)}...);
                else
                    return ::new (p) T(proxy_wrap(__y, std::forward<A>(a))...);
            }
    };

/**
    @brief Subscript whose form is only known at instantiation.

    Calls @c __operator_subscript when @c T provides one taking the proxy,
    and the built-in subscript otherwise.
*/
template <typename T, typename I>
    struct has_proxy_subscript
    {
        // Probes the renamed operator: a user operator[] is emitted as __operator_subscript(proxy, index).
        template <typename U>
            static auto probe(int)
                -> decltype(std::declval<U &>().__operator_subscript(
                                std::declval<node_proxy const &>(),
                                std::declval<I>()),
                            std::true_type());
        template <typename>
            static std::false_type probe(...);

        static constexpr bool value = decltype(probe<T>(0))::value;
    };

template <typename T, typename I>
    inline decltype(auto) subscript(node_proxy const & __y, T && t, I && i)
    {
        if constexpr (has_proxy_subscript<T, I>::value)
            return std::forward<T>(t).__operator_subscript(__y, std::forward<I>(i));
        else
            return std::forward<T>(t)[std::forward<I>(i)];
    }

/**
    @brief Converts an argument for an allocation function.

    Allocation functions keep their written types, so a @c root_ptr
    argument is converted back to a raw pointer.
*/
// One function with if constexpr: an overload pair would never select the
// root_ptr form for a non-const lvalue.
template <typename U>
    inline decltype(auto) alloc_arg(U && u)
    {
        using D = typename std::decay<U>::type;
        if constexpr (! std::is_pointer<D>::value &&
                      std::is_convertible<D, void *>::value)
            return static_cast<void *>(u);          // root_ptr via operator T *
        else
            return std::forward<U>(u);
    }

/** @brief Address given to a placement new: the last pointer among its arguments. */
template <typename U>
    inline void * as_void_ptr(U && u)
    {
        using D = typename std::decay<U>::type;
        if constexpr (std::is_pointer<D>::value)
            return const_cast<void *>(static_cast<void const *>(u));
        else if constexpr (std::is_convertible<D, void *>::value)
            return static_cast<void *>(u);
        else
            return nullptr;
    }

template <typename... A>
    inline void * placement_address(A &&... a)
    {
        void * r = nullptr;
        ((as_void_ptr(a) ? (r = as_void_ptr(a)) : r), ...);
        return r;
    }

template <typename T, typename... A>
    struct has_proxy_alloc
    {
        template <typename U>
            static auto probe(int)
                -> decltype(U::operator new(sizeof(U),
                                            std::declval<node_proxy const &>(),
                                            std::declval<A>()...),
                            std::true_type());
        template <typename>
            static std::false_type probe(...);

        static constexpr bool value = decltype(probe<T>(0))::value;
    };

template <typename T, typename... A>
    struct has_plain_alloc
    {
        template <typename U>
            static auto probe(int)
                -> decltype(U::operator new(sizeof(U), std::declval<A>()...),
                            std::true_type());
        template <typename>
            static std::false_type probe(...);

        static constexpr bool value = decltype(probe<T>(0))::value;
    };

template <typename T, typename... A>
    inline void * allocate_at_raw(node_proxy const & __y, A &&... a)
    {
        if constexpr (has_proxy_alloc<T, A...>::value)
            return T::operator new(sizeof(T), __y, std::forward<A>(a)...);
        else if constexpr (has_plain_alloc<T, A...>::value)
            return T::operator new(sizeof(T), std::forward<A>(a)...);
        else
            // No member form. Every placement operator new returns its address argument,
            // so return that directly.
            return placement_address(std::forward<A>(a)...);
    }

/**
    @brief Allocation half of a placement new.

    Emitted as @c ::new @c (allocate_at<T>(__y, args...)) @c T(ctorargs...),
    so the class's own placement operator new still runs, with the proxy
    when it takes one, and the global placement new only constructs. The
    arguments are normalized first, so the probes and the calls see the
    same types.
*/
template <typename T, typename... A>
    inline void * allocate_at(node_proxy const & __y, A &&... a)
    {
        return allocate_at_raw<T>(__y, alloc_arg(std::forward<A>(a))...);
    }

} // namespace boost


namespace std
{

/** @brief Iterator traits for @c root_ptr, which stands in for raw pointers. */

template <typename T>
    struct iterator_traits<boost::root_ptr<T>>
    {
        typedef random_access_iterator_tag iterator_category;
        typedef typename remove_cv<T>::type value_type;
        typedef ptrdiff_t difference_type;
        typedef T * pointer;
        typedef typename add_lvalue_reference<T>::type reference;
    };

/** @brief @c std::remove_pointer for @c root_ptr, which stands in for @c T*. */

template <typename T>
    struct remove_pointer<boost::root_ptr<T>>
    {
        typedef T type;
    };

template <typename T>
    struct remove_pointer<boost::root_ptr<T> const>
    {
        typedef T type;
    };

template <typename T>
    struct remove_pointer<boost::root_ptr<T> volatile>
    {
        typedef T type;
    };

template <typename T>
    struct remove_pointer<boost::root_ptr<T> const volatile>
    {
        typedef T type;
    };

template <typename T>
    struct hash<boost::root_ptr<T>>
    {
        size_t operator() (boost::root_ptr<T> const & p) const
        {
            return reinterpret_cast<size_t>(p.get());
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
