/**
	@file
	Boost intrusive_stack.hpp header file.

	@note
	Copyright (c) 2008 Phil Bouchard <phil@fornux.com>.

	Distributed under the Boost Software License, Version 1.0.

	See accompanying file LICENSE_1_0.txt or copy at
	http://www.boost.org/LICENSE_1_0.txt

	See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#ifndef BOOST_INTRUSIVE_STACK_HPP_INCLUDED
#define BOOST_INTRUSIVE_STACK_HPP_INCLUDED


#include <boost/detail/roofof.hpp>


namespace boost
{

namespace detail
{

namespace bp
{


struct intrusive_stack_node
{
	intrusive_stack_node * next;
	
	void insert(intrusive_stack_node * const p)
	{
		p->next = next;
		next = p;
	}
};


class intrusive_stack_base
{
protected:
	intrusive_stack_node impl;
	
	intrusive_stack_base() 							{ clear(); }

	void clear()
	{
		impl.next = & impl;
	}
};


/**
	Static stack.
	
	Rewritten stack template with explicit access to internal nodes.  This 
	allows usages of tags already part of an object, used to group objects 
	together without the need of any memory allocation.
*/

class intrusive_stack : protected intrusive_stack_base
{
	typedef intrusive_stack_base					base;

public:
	typedef intrusive_stack_node					node;
	typedef intrusive_stack_node *					pointer;
	template <typename T, intrusive_stack_node T::* P> 
		struct iterator;

protected:
	using base::impl;

public:
	pointer begin() 								{ return impl.next; }
	pointer end() 									{ return & impl; }

	bool empty() const 								{ return impl.next == & impl; }
	
	void push(pointer i)
	{
		end()->insert(i);
	}
};


template <typename T, intrusive_stack_node T::* P>
	struct intrusive_stack::iterator
	{
		typedef iterator         						self_type;
		typedef intrusive_stack_node               		node_type;

		iterator() : node_() 							{}
		iterator(intrusive_stack::pointer __x) : node_(__x) {}

		T & operator * () const 						{ return * roofof(P, node_); }
		T * operator -> () const						{ return roofof(P, node_); }

		self_type & operator ++ ()
		{
			node_ = node_->next;
			return * this;
		}

		bool operator == (const self_type & x) const 	{ return node_ == x.node_; }
		bool operator != (const self_type & x) const 	{ return node_ != x.node_; }

		node_type * node_;
	};


} // namespace bp

} // namespace detail

} // namespace boost


#endif // #ifndef BOOST_INTRUSIVE_STACK_HPP_INCLUDED
