/**
	@file
	T100 brain kernel.

	@note
	Copyright (c) 2008 Phil Bouchard <phil@fornux.com>.

	Distributed under the Boost Software License, Version 1.0.

	See accompanying file LICENSE_1_0.txt or copy at
	http://www.boost.org/LICENSE_1_0.txt

	See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/


#ifndef T100_HPP_INCLUDED
#define T100_HPP_INCLUDED


#include <vector>
#include <string>
#include <iostream>
#include <boost/regex.hpp>
#include <boost/block_ptr.hpp>


namespace boost
{

namespace detail
{

namespace sh
{


struct neuron_base
{
    typedef boost::block_ptr<neuron_base> pointer;

    enum sense_t {sight, sound, touch, smell, taste};

    boost::regex exp_;
    //std::vector< std::pair<double, pointer> > sub_;
    std::pair<double, pointer> sub_[3];

    neuron_base(std::string const & s) : exp_(s)/*, sub_(exp_.mark_count())*/ {}
    virtual ~neuron_base() {};

    virtual double operator () (std::string const & input) { return 0; };
};


/**
    Core brain kernel.
    
    @note
    The brain kernel will have a thread running in the background to sort out 
    all confused neurons that didn't take any decisions yet.  This should be 
    linked with the imaginative section of the brain which guesses over and 
    over (trials & errors) when to many regex occurences are present.
*/

template <neuron_base::sense_t>
    class neuron : public neuron_base
    {
        // disable non-"block<neuron>" allocations:
        void * operator new (size_t);

    public:
        typedef boost::block<neuron> pointee;
        
        /** 
            The following should be one of these multimap indexed by:
            - regular expression string
            - weight
            - emotional state weight (normal, angry, or scared)
        */
        
        typedef std::map<std::string, pointer> map_sn_t;
        
        static map_sn_t search_;
    
        neuron(std::string const & s) : neuron_base(s)
        {
            /// FIXME
            //search_[s] = (pointee *) (typename pointee::roofof) static_cast<neuron *>(rootof<is_polymorphic<neuron>::value>::get(this));

            //if (p1) sub_[0].second = p1;
            //if (p2) sub_[1].second = p2;
            //if (p3) sub_[2].second = p3;
        }

        double operator () (std::string const & input)
        {
            boost::match_results<std::string::const_iterator> what;

            if (! boost::regex_match(input, what, exp_, boost::match_default | boost::match_partial))
                return 0;
            
            if (! what[0].matched)
                return 0;

            // ponderate
            double accuracy = what.size() > 1 ? 0 : 1;
            for (unsigned i = 1; i < what.size(); i ++)
            {
                if (what[i].matched)
                {
                    sub_[i - 1].first = (* sub_[i - 1].second)(what[i].str()) / (what.size() - 1);
                    accuracy += sub_[i - 1].first;
                }
            }

/*
            if (accuracy < .7)
                return accuracy;
*/

            // learn if sounds equitable, God tells you to or "energy" spent is still low
            for (unsigned i = 1; i < what.size(); i ++)
                if (what[i].matched)
                {
                    if (sub_[i - 1].first == 0)
                    {
                        typename map_sn_t::iterator j = search_.find(what[i].str());
                        
                        if (j != search_.end())
                        {
                            /**
                                What we should do here is to:
                                - cummulate suggestions
                                - calculate difference between all proposals
                                - create new regular expression when demand is too high
                            */
                            sub_[i - 1].second = j->second;
                        }
                        else
                        {
                            /**
                                Learn
                            */
                            sub_[i - 1].second->exp_ = sub_[i - 1].second->exp_.str() + "|" + input;
                        }
                    }
                }
            
            return accuracy;
        }
    };

template <neuron_base::sense_t I>
    typename neuron<I>::map_sn_t neuron<I>::search_;


typedef neuron<neuron_base::sight> neuron_sight;
typedef neuron<neuron_base::sound> neuron_sound;
typedef neuron<neuron_base::touch> neuron_touch;
typedef neuron<neuron_base::smell> neuron_smell;
typedef neuron<neuron_base::taste> neuron_taste;


} // namespace sh

} // namespace detail

} // namespace boost


#endif
