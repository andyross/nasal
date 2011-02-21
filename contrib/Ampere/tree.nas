# ********** ********** ********** ********** ********** ********** ********** ********** ********** **********
# Copyright (C) 2005  Ampere K. [Hardraade]
#
# This file is protected by the GNU Public License.  For more details, please see the text file COPYING.
# ********** ********** ********** ********** ********** ********** ********** ********** ********** **********
# tree.nas
# This is a tree data-type implemented in Nasal, for allowing one to store elements hirearchically.
#
# Class:
#  Tree
#  	Methods:
#  	 new()		- creates and returns a new instance of the tree object.
#  	 child(v, i)	- returns the i-th child of the specified node, if it exists.
#  	 children(v)	- returns the children of node v in a vector.
#  	 element(v)	- returns the element that is stored in node v.
#  	 insert(v, e)	- creates a new node for element e, and inserts the new node as a child of node v.
#  	 isEmpty()	- returns whether the tree is empty.
#  	 isInternal(v)	- returns whether node v is an internal node.
#  	 isExternal(v)	- returns whether node v is an external node.
#  	 isRoot(v)	- returns whether node v is the root of this tree.
#  	 parent(v)	- returns the parent of node v.
#  	 remove(v)	- removes node v from the tree, and returns the element that was stored by the node.
#  	 replace(v, e)	- replaces the element in v with e.
#  	 root() 	- returns the root of this tree.
#  	 size() 	- returns the number of nodes contained in the tree.
# ********** ********** ********** ********** ********** ********** ********** ********** ********** **********
Tree = {
# Private objects:
	Node : {
	# Methods:
		# Creates and returns a new instance of the node object.
		new : func(index, element, parent){
			obj = {};
			obj.parents = [Tree.Node];
			
			# Instance variables:
			obj._children = [];
			obj._element = element;
			obj._index = index;
			obj._isRoot = 0;
			obj._parent = parent;
			
			return obj;
		},
		
	# Auxiliary methods:
	# Queries:
		# Returns whether this is a root.
		isRoot : func{
			return me._isRoot;
		},
		
	# Accessors:
		# Returns a vector containing the children.
		children : func{
			return me._children;
		},
		
		# Returns the element being stored.
		element : func{
			return me._element;
		},
		
		# Returns the index assigned to this node.
		index : func{
			return me._index;
		},
		
		# Returns the parent of this node.
		parent : func{
			return me._parent;
		},
		
	# Modifiers:
		# Sets this node as the root.
		setAsRoot : func{
			# This node can only be the root if it has no parent or children.
			if (me._parent == nil and size(me._children) == 0){
				me._isRoot = 1;
				return 1;
			}
			return 0;
		},
		
		# Replaces the current element with e.
		setElement : func(e){
			tmp = me._element;
			me._element = e;
			return tmp;
		}
	},

# Methods:
	# Creates and returns a new instance of the tree object.
	new : func{
		obj = {};
		obj.parents = [Tree];
		
		# Instance varaibles:
		obj._root = obj.Node.new(0, nil, nil);
		obj._root.setAsRoot();
		obj._size = 0;
		
		return obj;
	},
	
# Auxiliary methods:
	# Vertifies that the given position is valid.
	checkPosition : func(v){
		if (v.isRoot()){
			return v;
		}
		if (v.parent() == nil and size(v.children())){
			# Raise an error.
			# DO NOT REMOVE!
			die ("The node is not a part of the tree.");
		}
		return v;
	},

# Queries:
	# Returns whether the tree is empty.
	isEmpty : func{
		return (me._size == 0);
	},
	
	# Checks whether the specified node is an internal node.
	isInternal : func(v){
		me.checkPosition(v);
		return (size(v.children()) > 0);
	},
	
	# Checks whether the specified node is an external node.
	isExternal : func(v){
		me.checkPosition(v);
		return (size(v.children()) == 0);
	},
	
	# Checks whether the specified node is the root.
	isRoot : func(v){
		return (v.isRoot());
	},
	
	# Returns the number of nodes being stored in the tree.
	size : func{
		return me._size;
	},

# Accessors:
	# Returns the i-th child of the specified node, if it exists.
	child : func(v, i){
		me.checkPosition(v);
		
		# Get the list of children.
		c = v.children();
		
		# Make sure i is not out of bound.
		if (i >= size(c)){
			return nil;
		}
		return c[i];
	},
	
	# Returns a vector of children of the specified node.
	children : func(v){
		me.checkPosition(v);
		
		# Return a copy of the vector.  We don't want the user to have the ability to mess up the
		#  structure of the tree.
		return subvec(v.children(), 0);
	},
	
	# Returns the element of contained in the node.
	element : func(v){
		me.checkPosition(v);
		return v.element();
	},
	
	# Returns the parent of the specified node.
	parent : func(v){
		me.checkPosition(v);
		return v.parent();
	},
	
	# Returns the root of the tree.
	root : func{
		return me._root;
	},

# Modifiers:
	# Creates and returns a new node to store element e, then inserts the new node at the given postion of
	#  the tree.
	insert : func(v, e){
		me.checkPosition(v);
		
		# Create a new node.
		n = me.Node.new(size(v.children()), e, v);
		
		# Appends the new node to the children of v.
		append(v.children(), n);
		
		me._size = me._size + 1;
		
		# Returns the new node.
		return n;
	},
	
	# Removes the specified node from the tree, and returns the element that was stored by the node.
	remove : func(v){
		# We can't remove the root.
		if (v.isRoot()){
			# Raise an error.
			# DO NOT REMOVE!
			die ("Root cannot be removed from the tree.");
		}
		else {
			me.checkPosition(v);
		}
		
		# We can only remove external node.
		if (me.isInternal(v)){
			# Raise an error.
			# DO NOT REMOVE!
			die ("Internal node cannot be removed from the tree.");
		}
		
		# Remove node v using its index.
		parent = v.parent();
		siblings = parent.children();
		siblingCount = size(siblings);
		tmp = siblings[v.index()].element();
		for (i = v.index(); i < siblingCount - 1; i = i + 1){
			siblings[i] = siblings[i + 1];
		}
		
		setsize(siblings, siblingCount - 1);
		
		# We want to remove v's references to its parent and children, but the node class doesn't 
		#  provide us a direct way for doing this.  So, we will create a new node and copy v's index
		#  and element over.
		v = me.Node.new(v.index(), v.element(), nil);
		
		me._size = me._size - 1;
		
		return tmp;
	},
	
	# Replaces the element in the specified node with e.
	replace : func(v, e){
		me.checkPosition(v);
		
		tmp = v.element();
		v.setElement(e);
		return tmp;
	}
};