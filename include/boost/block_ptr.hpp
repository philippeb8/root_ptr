/**
	@file
	Boost block_ptr.hpp header file.

	@author
	Copyright (c) 2008 Phil Bouchard <phil@fornux.com>.

	@note
	Distributed under the Boost Software License, Version 1.0.

	See accompanying file LICENSE_1_0.txt or copy at
	http://www.boost.org/LICENSE_1_0.txt

	See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.


	Thanks to: Steven Watanabe <watanabesj@gmail.com>
*/


#ifndef BOOST_BLOCK_PTR_INCLUDED
#define BOOST_BLOCK_PTR_INCLUDED


#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4355 )

#include <new.h>
#endif

#include <iostream>
#include <boost/pool/pool_alloc.hpp>
#include <boost/type_traits/add_pointer.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>

#include <boost/detail/intrusive_list.hpp>
#include <boost/detail/intrusive_stack.hpp>
#include <boost/detail/roofof.hpp>
#include <boost/detail/block_ptr_base.hpp>


namespace boost
{

namespace detail
{

namespace bp
{


class block_base;


/**
	Set header.
	
	Proxy object used to count the number of pointers from the stack are referencing pointee objects belonging to the same @c block_header .
*/

struct block_header
{
    long count_;								/**< Count of the number of pointers from the stack referencing the same @c block_header .*/
    mutable block_header * redir_;					/**< Redirection in the case of an union multiple sets.*/

	bool destroy_;									/**< Destruction sequence initiated. */
    intrusive_list::node tag_;						/**< Tag used to enlist to @c block_header::includes_ . */

    intrusive_list includes_;						/**< List of all sets of an union. */
    intrusive_list elements_;						/**< List of all pointee objects belonging to a @c block_header . */

	static mutex & static_mutex()					/**< Main global mutex used for thread safety */
	{
		static mutex mutex_;
		
		return mutex_;
	}

	static fast_pool_allocator<block_header> & static_pool() /**< Pool where all sets are allocated. */
	{
    	static fast_pool_allocator<block_header> pool_;
    	
    	return pool_;
    }

	/**
		Initialization of a single @c block_header .
	*/
	
    block_header() : count_(1), redir_(this), destroy_(false)
    {
        includes_.push_back(& tag_);
    }

	
	/**
		Release of a @c block_header with possible destruction of all its elements and other sets unified to it.
		
		@return		True if the @c block_header was released.
	*/
	
    bool release()
    {
        if (-- count_ == 0)
        {
			destroy_ = true;
			
            for (intrusive_list::iterator<block_base, & block_base::block_tag_> i; i = elements_.begin(), i != elements_.end(); )
                delete &* i;
                
			destroy_ = false;
            
            for (intrusive_list::iterator<block_header, & block_header::tag_> i = includes_.begin(), j; j = i, i != includes_.end(); i = j)
			{ 
				++ j;
				
                if (&* i != this)
                    delete &* i;
			}

            return true;
        }

        return false;
    }

	
	/**
		Search for the @c block_header header of an union.
		
		@return		@c block_header responsible for managing the counter of an union.
	*/
	
    block_header * redir() const
    {
        while (redir_ != redir_->redir_)
        	redir_ = redir_->redir_;
        
        return redir_;
    }
	
	
	/**
		Unification with a new @c block_header .
		
		@param	p	New @c block_header to unify with.
	*/

    void redir(block_header * p)
    {
        if (redir_ != p)
        {
            redir_ = p;
            redir_->includes_.merge(includes_);
            redir_->elements_.merge(elements_);
            redir_->count_ += count_;
        }
    }

	
	/**
		Allocates a new @c block_header using the fast pool allocator.
		
		@param	s	Size of the @c block_header .
		@return		Pointer of the new memory block.
	*/
	
    void * operator new (size_t s)
    {
        return static_pool().allocate(s);
    }
    
	
	/**
		Placement new.
		
		@param	s	Size of the @c block_header .
		@param	p	Address to construct the @c block_header on.
		@return		Address to construct the @c block_header on.
	*/
	
    void * operator new (size_t s, block_header * p)
    {
        return p;
    }

	
	/**
		Deallocates a @c block_header from the fast pool allocator.
		
		@param	p	Address of the @c block_header to deallocate.
	*/
	
    void operator delete (void * p)
    {
        static_pool().deallocate(static_cast<block_header *>(p), sizeof(block_header));
    }
};


#define TEMPLATE_DECL(z, n, text) BOOST_PP_COMMA_IF(n) typename T ## n
#define ARGUMENT_DECL(z, n, text) BOOST_PP_COMMA_IF(n) T ## n const & t ## n
#define PARAMETER_DECL(z, n, text) BOOST_PP_COMMA_IF(n) t ## n

#define BEFRIEND_MAKE_BLOCK(z, n, text)																			    	\
	template <typename V, BOOST_PP_REPEAT(n, TEMPLATE_DECL, 0)>										                    \
		friend block_ptr<V> text(BOOST_PP_REPEAT(n, ARGUMENT_DECL, 0));

#define CONSTRUCT_MAKE_BLOCK(z, n, text)																			    \
	template <typename V, BOOST_PP_REPEAT(n, TEMPLATE_DECL, 0)>										                    \
		block_ptr<V> text(BOOST_PP_REPEAT(n, ARGUMENT_DECL, 0))															\
		{																												\
			return block_ptr<V>(new block<V>(BOOST_PP_REPEAT(n, PARAMETER_DECL, 0)));									\
		}


/**
	Deterministic memory manager of constant complexity.
	
	Complete memory management utility on top of standard reference counting.
*/

template <typename T>
    class block_ptr : public block_ptr_base<T>
    {
        template <typename> friend class block_ptr;

        typedef block_ptr_base<T> base;
        
        using base::share;
		using base::po_;

        union
        {
            block_header * ps_;						/**< Pointer to the @c block_header node @c block_ptr<> belongs to. */
            intrusive_stack::node pn_;				/**< Tag used for enlisting a pointer on the heap to later share the @c block_header it belongs to. */
        };

	public:
		/**
			Initialization of a pointer living on the stack or proper enlistment if living on the heap.
			
			@param	p	New pointee object to manage.
		*/
		
        template <typename V>
            block_ptr(block<V> * p) : base(p)
            {
                if (! pool::is_from(this))
                {
                    ps_ = new block_header();

                    init(p);
                }
                else
                {
                    pool::top(this)->ptrs_.push(& pn_);
                    pool::top(this)->inits_.merge(p->inits_);
                }
            }

		
		/**
			Assignment & union of 2 sets if the pointee resides a different @c block_header.
			
			@param	p	New pointee object to manage.
		*/
		
        template <typename V>
            block_ptr & operator = (block<V> * p)
            {
#ifndef BOOST_DISABLE_THREADS
       			mutex::scoped_lock scoped_lock(block_header::static_mutex());
#endif

                release(false);

                init(p);

                base::operator = (p);

                return * this;
            }

        template <typename V>
	        void reset(block<V> * p)
	        {
	            operator = <T>(p);
	        }
            
		template <typename V>
			friend block_ptr<V> make_block();

		BOOST_PP_REPEAT_FROM_TO(1, 10, BEFRIEND_MAKE_BLOCK, make_block)

    public:
        typedef T                       value_type;
        typedef block<value_type>     element_type;


		/**
			Initialization of a pointer living on the stack or proper enlistment if living on the heap.
		*/
		
        block_ptr() : ps_(0)
        {
            if (! pool::is_from(this))
                ps_ = new block_header();
            else
                pool::top(this)->ptrs_.push(& pn_);
        }

		
		/**
			Initialization of a pointer living on the stack or proper enlistment if living on the heap.
			
			@param	p	New pointer to manage.
		*/

        template <typename V>
            block_ptr(block_ptr<V> const & p) : base(p)
            {
                if (! pool::is_from(this))
                    ps_ = new block_header();
                else
                    pool::top(this)->ptrs_.push(& pn_);

                ps_->redir()->redir(p.ps_->redir());
            }

		
		/**
			Initialization of a pointer living on the stack or proper enlistment if living on the heap.
			
			@param	p	New pointer to manage.
		*/

			block_ptr(block_ptr<T> const & p) : base(p)
            {
                if (! pool::is_from(this))
                    ps_ = new block_header();
                else
                    pool::top(this)->ptrs_.push(& pn_);
				
                ps_->redir()->redir(p.ps_->redir());
            }


		/**
			Assignment & union of 2 sets if the pointee resides a different @c block_header.
			
			@param	p	New pointer to manage.
		*/
			
        template <typename V>
            block_ptr & operator = (block_ptr<V> const & p)
            {
#ifndef BOOST_DISABLE_THREADS
       			mutex::scoped_lock scoped_lock(block_header::static_mutex());
#endif

                if (ps_->redir() != p.ps_->redir())
                {
                    release(false);
					
                	ps_->redir()->redir(p.ps_->redir());
                }
                base::operator = (p);

                return * this;
            }


		/**
			Assignment & union of 2 sets if the pointee resides a different @c block_header.
			
			@param	p	New pointer to manage.
		*/

        block_ptr & operator = (block_ptr<T> const & p)
        {
            return operator = <T>(p);
        }

        void reset()
        {
#ifndef BOOST_DISABLE_THREADS
       		mutex::scoped_lock scoped_lock(block_header::static_mutex());
#endif

            release(false);
        }
        
        template <typename V>
	        void reset(block_ptr<V> const & p)
	        {
	            operator = <T>(p);
	        }
        
        bool cyclic() const
        {
        	return ps_->redir()->destroy_;
        }

        ~block_ptr()
        {
			if (cyclic())
				base::po_ = 0;
			else
				release(true);
        }

    private:
		/**
			Release of the pointee object with or without destroying the entire @c block_header it belongs to.
			
			@param	d	Destroy (true) or reuse (false) the @c block_header it is releasing.
		*/
		
        void release(bool d)
        {
            base::reset();
            
            if (! pool::is_from(this))
            {
    			block_header * p = ps_->redir();
    	
                if (p->release())
                    if (! d)
                    {
                    	p->~block_header();
                        ps_ = new (p) block_header();
                    }
                    else
                        delete p;
                else 
					if (! d)
                    	ps_ = new block_header();
            }
        }

		
		/**
			Enlist & initialize pointee objects belonging to the same @c block_header .  This initialization occurs when a pointee object is affected to the first pointer living on the stack it encounters.
			
			@param	p	Pointee object to initialize.
		*/
		
        void init(block_base * p)
        {
            if (p->init_)
                return;

    		block_header * q = ps_->redir();
    	
			// iterate memory blocks
            for (intrusive_list::iterator<block_base, & block_base::init_tag_> i = p->inits_.begin(); i != p->inits_.end(); ++ i)
            {
                i->init_ = true;
                q->elements_.push_back(& i->block_tag_);

				// iterate block_ptr elements
                for (intrusive_stack::iterator<block_ptr, & block_ptr::pn_> j = i->ptrs_.begin(), k; k = j, j != i->ptrs_.end(); j = k)
				{
					++ k;
                    j->ps_ = ps_;
				}
            }
        }

#if defined(BOOST_HAS_RVALUE_REFS)
	public:
    	block_ptr(block_ptr<T> && p): base(p.po_), ps_(p.ps_)
    	{
    		p.po_ = 0;
    	}

    	template<class Y>
    		block_ptr(block_ptr<Y> && p): base(p.po_), ps_(p.ps_)
    		{
    			p.po_ = 0;
    		}

    	block_ptr<T> & operator = (block_ptr<T> && p)
    	{
			std::swap(po_, p.po_);
			std::swap(ps_, p.ps_);
       		
        	return *this;
    	}

    	template<class Y>
    		block_ptr & operator = (block_ptr<Y> && p)
    		{
				std::swap(po_, p.po_);
				std::swap(ps_, p.ps_);
        		
        		return *this;
    		}
#endif
    };

template <typename V>
	block_ptr<V> make_block()
	{
		return block_ptr<V>(new block<V>());
	}

BOOST_PP_REPEAT_FROM_TO(1, 10, CONSTRUCT_MAKE_BLOCK, make_block)

} // namespace bp

} // namespace detail

using detail::bp::block_ptr;
using detail::bp::block;
using detail::bp::make_block;

} // namespace boost


#if defined(_MSC_VER)
#pragma warning( pop )
#endif


#endif // #ifndef BOOST_BLOCK_PTR_INCLUDED
