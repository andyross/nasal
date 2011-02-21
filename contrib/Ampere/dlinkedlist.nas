# ********** ********** ********** ********** ********** ********** ********** ********** ********** **********
# Copyright (C) 2005  Ampere K. [Hardraade]
#
# This file is protected by the GNU Public License.  For more details, please see the text file COPYING.
# ********** ********** ********** ********** ********** ********** ********** ********** ********** **********
# DLinkedList.nas
# This is a doubly-linked-list implemented using Nasal.  A linked-list basically serves the same purpose as a
#  vector (an array) by storing a list of elements.  Unlike a vector however, a doubly-linked-list allows fast
#  insertion and removal of elements, but with severe performance lost in look up functions.
# There are two types of linked-list: singly-linked-list and doubly-linked-list.  In singly-linked-list, one
#  can only traverse the list in one direction.  A doubly-linked-list allows traversal in both directions:
#  traversal can be done in-order by starting from the first element in the list, or in reversed-order by
#  starting from the last element in the list.  This Nasal script implements the latter type of linked-list.
#
# Class:
#  DLinkedList
#  	Methods:
#  	 new()			- creates and returns a new instance of the double linked list.
#  	 insertAfter(p, e)	- inserts the specified element after the given position.
#  	 insertBefore(p, e)	- inserts the specified element before the given position.
#  	 insertFirst(e)		- inserts the given element at the beginning of the list.
#  	 insertLast(e)		- inserts the given element at the end of the list.
#  	 isEmpty()		- returns whether the list is empty.
#  	 getElement(p)		- returns the element stored in the given node.
#  	 getFirst()		- returns the first node in the object.
#  	 getLast()		- returns the last node in the linked list.
#  	 getNext(p)		- returns the next node following p.
#  	 getPrev(p)		- returns the node preceding p.
#  	 remove(p)		- removes the specified node.
#  	 replace(p, e)		- replaces the element in the specified position with e.
#  	 size()			- returns the number of objects being stored in the list.
# ********** ********** ********** ********** ********** ********** ********** ********** ********** **********
DLinkedList = {
# Private objects:
	DNode : {
	# Methods:
		# Creates and returns a new instance of the node object.
		new : func(prev, next, element, isSentinal){
			obj = {};
			obj.parents = [DLinkedList.DNode];
			
			# Instance variables:
			obj._element = element;
			obj._isSentinal = isSentinal;
			obj._next = next;
			obj._prev = prev;
			
			return obj;
		},
		
	# Queries:
		# Returns whether this node is a sentinal.
		isSentinal : func{
			return me._isSentinal;
		},
		
	# Accessors:
		# Returns the element contained in the node.
		getElement : func{
			return me._element;
		},
		
		# Returns the next node.
		getNext : func{
			return me._next;
		},
		
		# Returns the previous node.
		getPrev : func{
			return me._prev;
		},
		
	# Modifiers:
		# Specifies the element being stored.
		setElement : func(element){
			me._element = element;
		},
		
		# Sets the node that follows this one.
		setNext : func(next){
			me._next = next;
		},
		
		# Sets the given node as the 
		setPrev : func(prev){
			me._prev = prev;
		}
	},
	
# Methods:
	# Creates and returns a new instance of the double linked list.
	new : func{
		obj = {};
		obj.parents = [DLinkedList];
		
		# Instance variables:
		obj._first = obj.DNode.new(nil, nil, nil, 1);
		obj._last = obj.DNode.new(obj._first, nil, nil, 1);
		obj._first.setNext(obj._last);
		obj._size = 0;
		
		return obj;
	},
	
	# Returns whether the list is empty.
	isEmpty : func{
		return (me._size == 0);
	},
	
	# Returns the number of objects being stored in the list.
	size : func{
		return me._size;
	},
	
# Auxiliary methods:
	# Checks whether the given position is valid.
	checkPosition : func(p){
		if (p == nil){
			die ("nil is an invalid position.");
		}
		if (p.isSentinal()){
			die ("The header/tail sentinal is an invalid position.");
		}
		if (p.getPrev() == nil and p.getNext() == nil){
			die ("Position does not belong to a double linked list.");
		}
		
		return p;
	},
	
# Accessors:
	# Returns the element contained in the given node.
	getElement : func(p){
		return p.getElement();
	},
	
	# Returns the node at the beginning of the list.
	getFirst : func{
		#if (me.isEmpty){
		#	# Force an error.  DO NOT REMOVE!
		#	die ("The List is Empty.");
		#}
		# If we have nothing to return, then return a nil.
		if (me.isEmpty()){
			return nil;
		}
		return me._first.getNext();;
	},
	
	# Returns the node at the end of the list.
	getLast : func{
		#if (me.isEmpty){
		#	# Force an error.  DO NOT REMOVE!
		#	die ("The List is Empty.");
		#}
		# If we have nothing to return, then return a nil.
		if (me.isEmpty()){
			return nil;
		}
		return me._last.getPrev();
	},
	
	# Returns the node that follows p, if it exists.
	getNext : func(p){
		v = me.checkPosition(p);
		
		next = nil;
		if (v != nil){
			next = v.getNext();
			if (next.isSentinal()){
				return nil;
			}
		}
		
		# This is not going to work.
		#if (next == me._last){
		#	die ("Cannot advance beyond the end of the list.");
		#}
		return next;
	},
	
	# Returns the node infront of p, if it exists.
	getPrev : func(p){
		v = me.checkPosition(p);
		
		prev = nil;
		if (v != nil){
			prev = v.getPrev();
			if (prev.isSentinal()){
				return nil;
			}
		}
		
		# This is not going to work.
		#if (prev == me._first){
		#	die ("Cannot advance past the beginning of the list.");
		#}
		return prev;
	},
	
# Modifiers:
	# Inserts the specified element behind the given position.
	insertAfter : func(p, e){
		v = me.checkPosition(p);
		
		newNode = me.DNode.new(v, v.getNext(), e, 0);
		#if (v.getNext() != nil){
		#	v.getNext().setPrev(newNode);
		#}
		#else {
		#	# New node will be our last node.
		#	me._last.setPrev(newNode);
		#}
		v.getNext().setPrev(newNode);
		v.setNext(newNode);
		
		me._size = me._size + 1;
		
		return newNode;
	},
	
	# Inserts the specified element infront of the given position.
	insertBefore : func(p, e){
		v = me.checkPosition(p);
		
		newNode = me.DNode.new(v.getPrev(), v, e, 0);
		#if (v.getPrev() != nil){
		#	v.getPrev().setNext(newNode);
		#}
		#else {
		#	# New node will be our first node.
		#	me._first.setNext(newNode);
		#}
		v.getPrev().setNext(newNode);
		v.setPrev(newNode);
		
		me._size = me._size + 1;
		
		return newNode;
	},
	
	# Inserts the specified element at the beginning of the list.
	insertFirst : func(e){
		me._size = me._size + 1;
		
		newNode = me.DNode.new(me._first, me._first.getNext(), e, 0);
		# Don't create link to sentinal.
		#newNode = me.DNode.new(nil, me._first.getNext(), e, 0);
		
		me._first.getNext().setPrev(newNode);
		me._first.setNext(newNode);
		
		return newNode;
	},
	
	# Inserts the specified element into the end of the list.
	insertLast : func(e){
		me._size = me._size + 1;
		
		newNode = me.DNode.new(me._last.getPrev(), me._last, e, 0);
		# Don't create link to sentinal.
		#newNode = me.DNode.new(me._last.getPrev(), nil, e, 0);
		
		me._last.getPrev().setNext(newNode);
		me._last.setPrev(newNode);
		
		return newNode;
	},
	
	# Removes the given position from the list, and returns its content.
	remove : func(p){
		v = me.checkPosition(p);
		
		me._size = me._size - 1;
		
		tmp = v.getElement();
		
		next = v.getNext();
		prev = v.getPrev();
		#if (next == nil){
		#	# This is the last node.
		#	me._last.setPrev(prev);
		#	prev.setNext(nil);
		#}
		#elsif (prev == nil){
		#	# This is the first node.
		#	me._first.setNext(next);
		#	next.setPrev(nil);
		#}
		#else {
			prev.setNext(next);
			next.setPrev(prev);
		#}
		
		v.setPrev(nil);
		v.setNext(nil);
		
		return tmp;
	},
	
	# Replaces the element in the specified position with the given element.
	replace : func(p, e){
		v = me.checkPosition(p);
		
		tmp = v.getElement();
		
		v.setElement(e);
		
		return tmp;
	}
};