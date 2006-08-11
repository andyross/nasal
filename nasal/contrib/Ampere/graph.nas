# ********** ********** ********** ********** ********** ********** ********** ********** ********** **********
# Copyright (C) 2005  Ampere K. [Hardraade]
#
# This file is protected by the GNU Public License.  For more details, please see the text file COPYING.
# ********** ********** ********** ********** ********** ********** ********** ********** ********** **********
# graph.nas
# This Nasal script is a library containing different implementations of the graph data-type.  A graph is
#  composed of a set of vertices V, and a collection of edges E each linking a pair of vertices.  Thus, a graph
#  can be used to represent connections or relationships between the vertices in set V.  This makes a graph
#  especially useful in FlightGear for modelling such things as the electrical system or hydraulic system.
# Currently, this graph library only contains an implementation called the "adjacency matrix".  In the future,
#  this library may expand to include such implementations as the "edge list" and "adjacency list" structures.
#
# Dependencies:
#  DLinkedList
#
# Class:
#  Graph
#  	Methods:
#  	 areAdjacent(v, w)	- returns whether vertices v and w are connected by the same edge.
#  	 edges()		- returns a vector containing all the edges in the graph.
#  	 endVertices(e)		- returns a vector containing the vertices connected by edge e.
#  	 incidentEdges(v)	- returns a vector containing all the edges connecting to vertex v.
#  	 insertEdge(v, w, x)	- creates and returns a new edge storing element x, and connecting vertices v
#  	 			   and w.
#  	 insertVertex(x)	- inserts a new vertex containing element x.
#  	 opposite(v, e)		- returns the vertex connected by e and distinct from v.
#  	 removeEdge(e)		- removes the specified edge from the graph.
#  	 removeVertex(v)	- removes the specified vertex from the graph.
#  	 replaceE(e, x)		- replaces edge e with edge x.
#  	 replaceV(v, x)		- replaces vertex v with vertex x.
#  	 vertices()		- returns a vector containing all the vertices in the graph.
#  Graph.AdjMatrix
#  	Methods:
#  	 new()			- creates and returns a new instance a graph object implemented using the 
#  	 			   "adjacency matrix" structure.
# ********** ********** ********** ********** ********** ********** ********** ********** ********** **********

Graph = {
# Methods:
	areAdjacent : func(v, w){
	},
	
# Accessors:
	edges : func{
	},
	
	endVertices : func(e){
	},
	
	incidentEdges : func(v){
	},
	
	opposite : func(v, e){
	},
	
	vertices : func{
	},
	
# Modifiers:
	insertEdge : func(v, w, x){
	},
	
	insertVertex : func(x){
	},
	
	removeEdge : func(e){
	},
	
	removeVertex : func(v){
	},
	
	replace : func(e, x){
	},
	
	replace : func(v, x){
	}
};

Graph.AdjMatrix = {
# Private classes:
	# This is an object for representing an edge.
	Incidence : {
	# Methods:
		# Creates and returns a new edge that connects the vertex v and w, and holds element x.
		new : func(v, w, x){
			obj = {};
			obj.parents = [Graph.AdjMatrix.Incidence];
			
			# Instance variables:
			obj._container = nil;
			obj._element = x;
			obj._v1 = v;
			obj._v2 = w;
			
			return obj;
		},
		
	# Auxiliary Methods:
	# Acessors:
		# Returns the container of this edge object.
		getContainer : func{
			return me._container;
		},
		
		# Returns the element being stored.
		getElement : func{
			return me._element;
		},
		
		# Returns the first vertex being connected.
		v1 : func{
			return me._v1;
		},
		
		# Returns the second vertex being connected.
		v2 : func{
			return me._v2;
		},
		
	# Modifiers:
		# Sets the container for this edge object.
		setContainer : func(c){
			tmp = me._container;
			me._container = c;
			
			return tmp;
		},
		
		# Stores the given element and returns the old one.
		setElement : func(e){
			tmp = me._element;
			me._element = e;
			
			return tmp;
		}
	},
	
	# This is a class that represents a matrix.
	Matrix : {
	# Methods:
		# Creates and returns a new matrix of size m x n.
		new : func(m, n){
			obj = {};
			obj.parents = [Graph.AdjMatrix.Matrix];
			
			# Instance variables:
			obj._rowCount = m;
			obj._colCount = n;
			obj._cell = [];
			
			setsize(obj._cell, m*n);
			
			return obj;
		},
		
	# Auxiliary methods:
		# Check if (i, j) is a valid position.
		isValidPos : func(i, j){
			if (i <= 0 or i > me._rowCount or j <= 0 or j > me._colCount){
				# Raise an error.
				# DO NOT REMOVE!
				die ("Position (" ~ i ~ ", " ~ j ~ ") is not a valid position in the Matrix.");
			}
			return 1;
		},
		
	# Queries:
		# Returns the number of columns.
		colCount : func{
			return me._colCount;
		},
		
		# Returns 1 if the cell (i, j) is empty.
		isEmpty : func(i, j){
			return (me.getElement(i, j) == nil);
		},
		
		# Returns the number of rows.
		rowCount : func{
			return me._rowCount;
		},
		
		# Returns the total number of cells in the matrix.
		size : func{
			return (me._colCount * me._rowCount);
		},
		
	# Accessors:
		# Returns the element stored at (i, j).
		getElement : func(i, j){
			me.isValidPos(i, j);
			return (me._cell[(i - 1) * me._colCount + j - 1]);
		},
		
		# Returns a submatrix starting from (i, j) to (k, l).
		subMatrix : func(i, j, k, l){
			if (i < 0 or j < 0 or k < i or l < j or k > me._rowCount or l > me._colCount){
				# Invalid range entered.
				die ("invalid range specified in subMatrix(i, j, k, l)");
			}
			
			# Create a new matrix.
			tmp = me.new(k - i + 1, l - j + 1);
			
			# Copy the contents to the new matrix.
			for (m = i; m <= k; m = m + 1){
				for (n = j; n <= l; n = n + 1){
					tmp.setElement(me.getElement(m, n), m - i + 1, n - j + 1);
				}
			}
			
			return tmp;
		},
	
	# Modifiers:
		# Inserts the given matrix into this matrix at location (i, j).  Truncate the given matrix if 
		#  it is too large.
		insertMatrix : func(m, i, j){
			# This new matrix will hold the displaced values.
			tmp = me.new(m.rowCount(), m.colCount());
			
			# Raise an error if m is invalid.
			if (m == nil){
				# Raise an error.  DO NOT REMOVE!
				die ("m in insertMatrix(m, i, j) is invalid.");
			}
			
			for (k = 0; k < m.rowCount(); k = k + 1){
				# Skip if k exceeded the limit.
				if ((k + i) > me._rowCount){
					return tmp;
				}
				for (l = 0; l < m.colCount(); l = l + 1){
					# Skip if l exceeded the limit.
					if ((l + j) > me._colCount){
						l = m.colCount();
					}
					
					# Copy element from m(k + 1, l + 1) to me(i + k, j + l).
					tmp.setElement(me.setElement(m.getElement(k + 1, l + 1), i + k, j + l), k + 1, l + 1);
				}
			}
			
			# Return the displaced elements in a new matrix.
			return tmp;
		},
		
		# Place the element e in cell (i, j), and returns the old element.
		setElement : func(e, i, j){
			me.isValidPos(i, j);
			
			# Swap the old element with the new one.
			tmp = me.getElement(i, j);
			me._cell[(i - 1) * me._colCount + j - 1] = e;
			
			return tmp;
		}
	},
	
	# A vertex object to represent the nodes in the graph.
	Vertex : {
	# Methods:
		# Creates and returns a new vertex object having an index i, storing element e.
		new : func(i, e){
			obj = {};
			obj.parents = [Graph.AdjMatrix.Vertex];
			
			# Instance variables:
			obj._container = nil;
			obj._element = e;
			obj._index = i;
			
			return obj;
		},
		
	# Auxiliary Methods:
	# Accessors:
		# Returns the container of this vertex object.
		getContainer : func{
			return me._container;
		},
		
		# Returns the element being stored.
		getElement : func{
			return me._element;
		},
		
		# Returns the user-defined index.
		getIndex : func{
			return me._index;
		},
		
	# Modifiers:
		# Sets the specified object as the container for this vertex object.
		setContainer : func(c){
			tmp = me._container;
			me._container = c;
			
			return tmp;
		},
		
		# Stores object e as an element, and returns the old element.
		setElement : func(e){
			tmp = me._element;
			me._element = e;
			
			return tmp;
		},
		
		# Applies a user-specified index to this vertex.
		setIndex : func(i){
			tmp = me._index;
			me._index = i;
			
			return tmp;
		}
	},
	
# Methods:
	# Creates and returns a new instance of a graph object.
	new : func(){
		obj = {};
		obj.parents = [Graph.AdjMatrix];
		
		# Instance variables:
		obj._edgeCount = 0;
		obj._edges = DLinkedList.new();
		obj._matrix = me.Matrix.new(0, 0);
		obj._vertices = DLinkedList.new();
		obj._vertexCount = 0;
		
		return obj;
	},
	
	# Returns whether vertices v and w are connected by the same edge.
	areAdjacent : func(v, w){
		# Let the index of v and w be vi-1 and wi-1.  For v and w to be adjacent:
		#  a) Matrix(vi, wi) (or Matrix(wi, vi)) must have a reference to an edge.
		#  b) The aforementioned edge must have a references to v and w.
		m = me._matrix;
		vi = v.getIndex() + 1;
		wi = w.getIndex() + 1;
		
		e = nil;
		if (vi > 0 and wi > 0 and vi <= m.rowCount() and wi <= m.rowCount()){
			e = m.getElement(vi, wi);
		}
		if (e == nil){
			return 0;
		}
		
		vj = e.v1().getIndex() + 1;
		wj = e.v2().getIndex() + 1;
		
		if (vi != vj and vi != wj){
			return 0;
		}
		
		return 1;
	},
	
# Auxiliary methods:
	# Vertifies that the given edge is valid.
	checkEdge : func(e){
		if (e == nil){
			# Raise an error.
			# DO NOT REMOVE!
			die ("nil is an invalid edge.");
		}
		
		# Check whether the edge can be found in our matrix.
		vi = e.v1().getIndex() + 1;
		vj = e.v2().getIndex() + 1;
		
		edge = me._matrix.getElement(vi, vj);
		if (edge == nil){
			# Raise an error.
			# DO NOT REMOVE!
			die ("the given edge is not part of this graph.");
		}
		wi = edge.v1().getIndex() + 1;
		
		if (vi != wi){
			# Raise an error.
			# DO NOT REMOVE!
			die ("the given edge is not part of this graph.");
		}
	},
	
	# Vertifies that the given vertex is valid.
	checkVertex : func(v){
		# Due to the way the vertices are stored in the graph, we can't perform detail checks without 
		#  severely degrade the performance of some functions.  Moreover, making more detail checks may
		#  duplicate the works done by other methods.  So, we will just make sure that v is not a nil
		#  value here.
		if (v == nil){
			# Raise an error.
			# DO NOT REMOVE!
			die ("nil is an invalid vertex.");
		}
	},
	
# Accessors:
	# Returns a vector containing all the edges in the graph.
	edges : func{
		tmp = [];
		setsize(tmp, me._edges.size());
		
		c = me._edges.getFirst();
		for (i = 0; c != nil; i = i + 1){
			tmp[i] = me._edges.replace(c, nil);
			me._edges.replace(c, tmp[i]);
			c = me._edges.getNext(c);
		}
		
		return tmp;
	},
	
	# Returns a vector containing the vertices connected by edge e.
	endVertices : func(e){
		if (e == nil){
			# Raise an error.
			# DO NOT REMOVE!
			die ("nil is an invalid edge.");
		}
		
		tmp = [e.v1(), e.v2()];
		
		return tmp;
	},
	
	# Returns a vector containing all the edges connecting to vertex v.
	incidentEdges : func(v){
		me.checkVertex(v);
		
		# Go through an entire column (or row) of our matrix, and put all the edges into a list.
		tmpList = DLinkedList.new();
		m = me._matrix;
		for (i = 1; i <= m.rowCount(); i = i + 1){
			edge = m.getElement(i, v.getIndex() + 1);
			
			if (edge != nil){
				tmpList.insertLast(edge);
			}
		}
		
		# Transfer the elements in the temporary list into an array.
		tmp = [];
		setsize(tmp, tmpList.size());
		c = tmpList.getFirst();
		for (i = 0; c != nil; i = i + 1){
			tmp[i] = tmpList.replace(c, nil);
			c = tmpList.getNext(c);
		}
		
		return tmp;
	},
	
	# Returns the vertex connected by e and distinct from v.
	opposite : func(v, e){
		me.checkVertex(v);
		
		if (e == nil){
			# Raise an error.
			# DO NOT REMOVE!
			die ("nil is an invalid edge.");
		}
		
		# One end of the edge e must be v.
		vi = v.getIndex();
		wi = e.v1().getIndex();
		wj = e.v2().getIndex();
		
		if (vi == wi){
			return e.v2();
		}
		else {
			if (vi == wj){
				return e.v1();
			}
		}
		
		return nil;
	},
	
	# Returns a vector containing all the vertices in the graph.
	vertices : func{
		tmp = [];
		setsize(tmp, me._vertices.size());
		
		c = me._vertices.getFirst();
		for (i = 0; c != nil; i = i + 1){
			tmp[i] = me._vertices.replace(c, nil);
			me._vertices.replace(c, tmp[i]);
			c = me._vertices.getNext(c);
		}
		
		return tmp;
	},
	
# Modifiers:
	# Inserts a new edge containing element x.
	insertEdge : func(v, w, x){
		me.checkVertex(v);
		me.checkVertex(w);
		
		# Create a new edge to connect v and w, then insert it into our container.
		e = me.Incidence.new(v, w, x);
		c = (me._edges.insertLast(e));
		e.setContainer(c);
		me._edgeCount = me._edgeCount + 1;
		
		# Using the indicies from v and w, create references from the matrix pointing to the newly
		#  created edge object.
		i = v.getIndex() + 1;
		j = w.getIndex() + 1;
		me._matrix.setElement(e, i, j);
		me._matrix.setElement(e, j, i);
		
		return e;
	},
	
	# Inserts a new vertex containing element x.
	insertVertex : func(x){
		# Create a new vertex and insert it into our container.
		v = me.Vertex.new(me._vertexCount, x);
		v.setContainer(me._vertices.insertLast(v));
		
		# Create a new matrix with an extra row and an extra column to our current matrix.
		me._vertexCount = me._vertexCount + 1;
		tmp = me.Matrix.new(me._vertexCount, me._vertexCount);
		
		# Copy the contents in the old matrix to the new one.
		tmp.insertMatrix(me._matrix, 1, 1);
		
		# Use the new matrix.
		me._matrix = tmp;
		
		return v;
	},
	
	# Removes the specified edge from the graph.
	removeEdge : func(e){
		me.checkEdge(e);
		
		# Remove e from the list.
		removed = me._edges.remove(e.getContainer());
		if (removed == nil){
			return nil;
		}
		
		# Remove references to e from the matrix.
		me._matrix.setElement(nil, removed.v1().getIndex() + 1, removed.v2().getIndex() + 1);
		me._matrix.setElement(nil, removed.v2().getIndex() + 1, removed.v1().getIndex() + 1);
		
		me._edgeCount = me._edgeCount - 1;
		
		return removed;
	},
	
	# Removes the specified vertex from the graph.
	removeVertex : func(v){
		me.checkVertex(v);
		
		# Modify the index of the rest of the vertices in the list.
		# Note: no need to make i = v.getIndex() + 1, since we are not dealing with a matrix.
		i = v.getIndex();
		c = me._vertices.getNext(v.getContainer());
		while (c != nil){
			vertex = me._vertices.replace(c, nil);
			vertex.setIndex(i);
			me._vertices.replace(c, vertex);
			c = me._vertices.getNext(c);
			i = i + 1;
		}
		
		# Remove v from the list.
		removed = me._vertices.remove(v.getContainer());
		if (removed == nil){
			return nil;
		}
		
		# Create a new matrix with one less row and one less column to our current matrix.
		tmp = me.Matrix.new(me._vertexCount - 1, me._vertexCount - 1);
		
		# Row i and Column i would divide the matrix into four quadrants.  Copy each quadarant 
		#  into our new matrix.
		
		rowShift = 0;
		# Go through all rows.
		for (k = 1; k <= me._vertexCount; k = k + 1){
			if (k != i + 1){
				colShift = 0;
				# Go through all columns.
				for (l = 1; l <= me._vertexCount; l = l + 1){
					if (l != i + 1){
						tmp.setElement(me._matrix.getElement(k, l), k + rowShift, l + colShift);
					}
					else {
						# Skip a column.
						colShift = -1;
					}
				}
			}
			else {
				# Skip a row.
				rowShift = -1;
			}
		}
		
		# Use the new matrix.
		me._matrix = tmp;
		
		# Decrease the vertex count.
		me._vertexCount = me._vertexCount - 1;
		
		return removed;
	},
	
	replaceE : func(e, x){
		if (e == nil){
			# Raise an error.
			# DO NOT REMOVE!
			die ("nil is an invalid edge.");
		}
		return e.setElement(x);
	},
	
	replaceV : func(v, x){
		me.checkVertex(v);
		return v.setElement(x);
	}
};