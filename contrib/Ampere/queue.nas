# ********** ********** ********** ********** ********** ********** ********** ********** ********** **********
# Copyright (C) 2005  Ampere K. [Hardraade]
#
# This file is protected by the GNU Public License.  For more details, please see the text file COPYING.
# ********** ********** ********** ********** ********** ********** ********** ********** ********** **********
# queue.nas
# This file contains two different implementations of a queue.  The first one, a dyanmic queue, is a queue
#  that has no theoretical upperbound in the amount of elements that can be stored.  The second one, a static
#  queue, has a fixed upperbound that needs to be specified during the creation of the queue object.
#
# To create a new instance of a dynamic queue, you can use the following line:
#  queue = Queue.Dyanmic.new();
#
# Similarily, you can create a new instance of a static queue by using the following line, with size as the
#  amount of objects that you would like the queue to be able to stored:
#  queue = Queue.Static.new([size]);
#
# Class:
#  Queue
#  Queue.Dynamic
#  	Methods:
#  	 new() 		-- creates a new instance of the queue object.
#  	 enqueue(obj) 	-- inserts an object at the rear of the queue.
#  	 dequeue() 	-- removes an object from the front of the queue.
#  	 front() 	-- takes a peak at the object at the front of the queue.
#  	 isEmpty() 	-- returns 0 if the queue is empty; 1 otherwise.
#  	 size() 	-- returns the amount of elements being queued.
#  Queue.Static
#  	Methods:
#  	 new(max)	-- creates a new instance of the queue object, with max as the size of the queue.
#  	 enqueue(obj) 	-- inserts the given object at the rear of the queue.  Returns 0 if unsuccessful.
#  	 dequeue() 	-- removes and returns an object from the front of the queue.
#  	 front() 	-- returns the object at the front of the queue without any removal.
#  	 getNth(n)	-- returns the n-th item in the queue.
#  	 isEmpty() 	-- returns 1 if the queue is empty; 0 otherwise.
#  	 size() 	-- returns the amount of elements in the queue.
# ********** ********** ********** ********** ********** ********** ********** ********** ********** **********
Queue = {};

Queue.Dynamic = {
# Sub-class:
	Node : {
		# Creates a new node with the given element and next node.
		new : func(e, n){
			obj = {};
			obj.parents = [Queue.Dynamic.Node];
			
			# Instance variables:
			obj.element = e;
			obj.next = n;
			
			return obj;
		},
		
		# Accessors:
		getElement : func{
			return me.element;
		},
		
		getNext : func{
			return me.next;
		},
		
		# Modifiers:
		setElement : func(e){
			me.element = e;
		},
		
		setNext : func(n){
			me.next = n;
		}
	},
	
# Methods:
	# Creates a new instance of the queue object.
	new : func{
		obj = {};
		obj.parents = [Queue.Dynamic];
		
		# Instance variables:
		obj._head = nil;
		obj._size = 0;
		obj._tail = nil;
		
		return obj;
	},
	
	# Inserts an object at the rear of the queue.
	enqueue : func(obj){
		# Set up a new node.
		node = me.Node.new(obj, nil);
		
		if (me._head == nil){
			# Special case where the queue is empty.
			
			# Point head as well as tail to this node.
			me._head = node;
			me._tail = node;
		}
		else {
			# Queue is not empty.
			
			# Set up the next reference of the tail.
			me._tail.setNext(node);
			# Advance tail.
			me._tail = node;
		}
		
		# Update the info on the current size.
		me._size = me._size + 1;
	},
	
	# Removes an object from the front of the queue.
	dequeue : func{
		if (me._head == nil){
			return nil;
		}
		else {
			# Advance the head pointer.
			tmp = me._head;
			me._head = me._head.getNext();
			
			# Update the info on the current size.
			me._size = me._size - 1;
			
			# Return the actual object being stored, not the node object.
			return tmp.getElement();
		}
	},
	
	# Takes a peek at the object at the front of the queue.
	front : func{
		# Returns the actual object being stored, not the node object.
		if (me._head == nil){
			return nil;
		}
		return me._head.getElement();
	},
	
	# Returns a boolean value indicating whether the queue is empty.
	isEmpty : func{
		return (me._head == nil);
	},
	
	# Returns the number of objects in the queue.
	size : func{
		return me._size;
	}
};

Queue.Static = {
# Methods:
	# Creates a new instance of the queue object.
	new : func(max){
		obj = {};
		obj.parents = [Queue.Static];
		
		# Instance variables:
		obj._head = 0;	# Sentinal.
		obj._max = max;
		obj._size = 0;
		obj._tail = 0;	# Sentinal.
		obj._queue = [nil];
		setsize(obj._queue, max + 1);
		
		return obj;
	},
	
# Queries:
	# Returns 1 if the queue is empty; 0 otherwise.
	isEmpty : func{
		return (me._size == 0);
	},
	
	# Returns the amount of elements in the queue.
	size : func{
		return me._size;
	},

# Auxiliary methods:
	# Increments the given counter, but wraps around if the counter exceeds the "max" variable.
	_increment : func(counter){
		counter = counter + 1;
		if (counter > me._max){
			counter = 0;
		}
	},

# Accessors:
	# Removes and dequeues object from the front of the queue.
	dequeue : func(){
		if (me._size <= 0){
			return nil;
		}
		
		# "Removal" and return the object at the front of the queue.
		tmp = me._queue[me._head];
		me._head = me._increment(me._head);
		me._size = me._size - 1;
		
		return tmp;
	},
	
	# Returns the object at the front of the queue without any removal.
	front : func{
		return me._queue[me._head];
	},
	
	# Returns the n-th item in the queue.
	getNth : func(n){
		if (n < 1 or n > me._size){
			return nil;
		}
		return me._queue[me._head + n - 1];
	}
	
# Modifiers:
	# Inserts the given object at the rear of the queue.  Returns 0 if unsuccessful.
	enqueue : func(obj){
		if (me._size > me._max - 1){
			return 0;
		}
		
		# Inserts the given object.
		me._queue[me._tail] = obj;
		me._size = me._size + 1;
		me._tail = me._increment(me._tail);
		
		return 1;
	}
};