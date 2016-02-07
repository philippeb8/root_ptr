/**
	@file
	t100_test1.cpp

	@note
	Copyright (c) 2008 Phil Bouchard <phil@fornux.com>.

	Distributed under the Boost Software License, Version 1.0.

	See accompanying file LICENSE_1_0.txt or copy at
	http://www.boost.org/LICENSE_1_0.txt

	See http://www.boost.org/libs/smart_ptr/doc/index.html for documentation.
*/

#include <vector>
#include <string>
#include <iostream>
#include <boost/regex.hpp>
#include <boost/block_ptr.hpp>

#include "t100.h"

using namespace std;
using namespace boost;
using boost::detail::sh::neuron_sight;


int main(int argv, char * argc[])
{
    block_ptr<neuron_sight> t100 = make_block<neuron_sight>("I eat ([a-z]+) then drink ([a-z]+)");
	t100->sub_[0].second = make_block<neuron_sight>("beef|chicken");
	t100->sub_[1].second = make_block<neuron_sight>("vodka|water");

    cout << (* t100)("I eat beef then drink wine") << endl;
    cout << (* t100)("I eat beef then drink wine") << endl;
}
