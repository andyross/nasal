#
# Typical little function
#
dist = func {
    x1 = arg[0]; y1 = arg[1];
    x2 = arg[2]; y2 = arg[2];
    dx = x2-x1;
    dy = y2-y1;
    return sqrt(dx*dx + dy*dy);
};

#
# Define a class object with one method, one field and one "new"
# contstructor.  The "new" function is not special in any way.
#
Class1 = {};

Class1.new = func {
    obj = { parents : (Class1),
            count : 0 };
    return obj;
};

Class1.getcount = func {
    me.count = me.count + 1;
    return me.count;
};

c = Class1.new();
c.getcount(); # returns 1
c.getcount(); # returns 2
c.getcount(); # returns 3

#
# This creates an identical class using alternative syntax.
#
Class2 = {
    new : func {
        obj = {};
	obj.parents = (Class2);
	obj.count = 0;
        return obj;
    },
    getcount : func {
        me.count = me.count + 1;
        return me.count;
    }
};

#
# Make a "inverted index" hash out of a vector that returns the index
# for each element.
#
invert = func {
    vec = arg[0];
    hash = {};
    for(i=0; i<size(vec); i++) {
        hash{vec[i]} = i;
    }
    return hash;
};

#
# Use the return value of the above function to do an "index of"
# lookup on a vector
#
vecfind = func{ return invert(arg[0]){arg[0]}; };

#
# Joins its arguments with the empty string and returns a scalar.
# Note use of "~" operator to do string concatenation (Nasl's only
# funny syntax).
#
join = func { 
    s = "";
    foreach(elem, arg) { s = s ~ elem; }
    return s;
};

#
# Functional programming A: All function expressions are inherently
# anonymous lambdas and can be used in mid-expression:
#
a = func{ arg[0] + 1 }(232);  # "a" now equals 233

#
# Functional programming B.  All expressions have a value, the last
# expression in a code block is the return value of that code block.
# There are no "statements" in NaSL, although some expressions
# (assignment, duh) have side effects.
#
factorial = func { if(arg[0] == 0) { 1 } else { frac(arg[0]); } };

#
# Functional programming C:  Lexical closures.
#
getcounter = func { count = 0; return func { count = count + 1 } };
mycounter = getcounter();
mycounter(); # Returns 1
mycounter(); # Returns 2
mycounter(); # Returns 3
mycounter(); # Returns 4...
