//
// Object.h
//
// Object: This baseclass defines how an object should behave.
//         This includes the ability to be put into a list
//
// Part of the ht://Dig package   <http://www.htdig.org/>
// Copyright (c) 1999 The ht://Dig Group
// For copyright details, see the file COPYING in your distribution
// or the GNU Public License version 2 or later 
// <http://www.gnu.org/copyleft/gpl.html>
//
// $Id: Object.h,v 1.6 1999/09/29 16:33:12 loic Exp $
//

#ifndef	_Object_h_
#define	_Object_h_

#include "lib.h"
#include <iostream.h>

class String;

class Object
{
public:
	//
	// Constructor/Destructor
	//
			Object()	{}
	virtual		~Object()	{}

	//
	// To ensure a consistent comparison interface and to allow comparison
	// of all kinds of different objects, we will define a comparison functions.
	//
	virtual int	compare(const Object &)	{ return 0;}

	//
	// To allow a deep copy of data structures we will define a standard interface...
	// This member will return a copy of itself, freshly allocated and deep copied.
	//
	virtual Object	*Copy() const { cerr << "Object::Copy: derived class does not implement Copy\n"; return new Object(); }
};


#endif
