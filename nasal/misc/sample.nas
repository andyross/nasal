# A no-op function used below to get this file to run.  Ignore and read on...
dummyFunc = func { 1 }

#
# Literal numbers can be decimal or exponential
#
n1 = 3;
n2 = 3.14;
n3 = 6.023e23;

#
# Two identical string literals with different quotes.  Double quotes
# use typical escape sequences.  Single quotes treat everything as
# literal except for embedded single quotes (including embedded
# whitespace like newlines).  Double quotes handle the following
# C-like escapes: \n \r \t \xnn \"
#
s1 = 'Andy\'s "computer" has a C:\righteous\newstuff directory.';
s2 = "Andy's \"computer\" has a C:\\righteous\\newstuff directory.";

#
# Literal lists use square brackets with a comma-separated expression
# list.
#
list1 = ["a", "b", 1, 2];

#
# Literal hashes (or objects -- same thing) use curlies and colons to
# separate the key/value pairs.  Note that the key can be either a
# symbol or a literal scalar.  Object definitions will typically want
# to use symbols, lookup tables of other types will be more
# comfortable with literals.
#
hash1 = { name : "Andy", job : "Hacker" };
EnglishEspanol = { "one" : "uno", "two": "dos", "blue" : "azul" };

#
# Both vectors and hashes use square brackets for the lookup operation:
#
list1[0] == "a";
hash1["name"] == "Andy";

#
# Typical little function.  Arguments are passed in the arg array, not
# unlike perl.  Note that this is just an assignment of an (anonymous)
# function argument to the local "dist" variable.  There is no
# function declaration syntax in Nasl.
#
sqrt = dummyFunc;
dist = func {
    x1 = arg[0]; y1 = arg[1];
    x2 = arg[2]; y2 = arg[3];
    dx = x2-x1;
    dy = y2-y1;
    return sqrt(dx*dx + dy*dy);
}
dist(0,0,1,1); # == sqrt(2)

#
# Nasl has no "statements", which means that any expression can appear
# in any context.  This means that you can use an if/else clause to do
# what the ?: does in C.  The last semicolon in a code block is
# optional, to make this prettier.
#
abs = func { if(arg[0] < 0) { -arg[0] } else { arg[0] } }

#
# Nasl supports a "nil" value for use as a null pointer equivalent.
# It can be tested for equality, matching only other nils.
#
listNode = { data : ["what", "ever"], next : nil };

#
# Nasl's binary boolean operators are "and" and "or", unlike C.
# unary not is still "!" however.  They short-circuit like you expect
#
toggle = 0;
a = nil;
if(a and a.field == 42) {
    toggle = !toggle; # doesn't crash when a is nil
}

#
# Looping constructs are mostly C-like.  The differences are that
# there is no do{}while(); construct, and there is a foreach, which
# takes a local variable name as its first argument and a vector as
# its second.
#
doSomething = dummyFunc;

stillGoing = 0;
while(stillGoing) { doSomething(); }

for(i=0; i < 3; i = i+1) {
    elem = list1[i];
    doSomething(elem);
}

foreach(elem; list1) { doSomething(elem) }  # Shorthand for above

#
# Define a class object with one method, one field and one "new"
# contstructor.  The "new" function is not special in any way; it
# simply returns an initialized object with the "parents" field set
# appropriately.  Member functions can get their local object (the
# equivalent of the "this" pointer in C++) as the "me" variable.
#
Class1 = {};

Class1.new = func {
    obj = { parents : [Class1],
            count : 0 };
    return obj;
}

Class1.getcount = func {
    me.count = me.count + 1;
    return me.count;
}

c = Class1.new();
print(c.getcount(), "\n"); # prints 1
print(c.getcount(), "\n"); # prints 2
print(c.getcount(), "\n"); # prints 3

#
# This creates an identical class using alternative syntax.
#
Class2 = {
    new : func {
        obj = {};
	obj.parents = [Class2];
	obj.count = 0;
        return obj;
    },
    getcount : func {
        me.count = me.count + 1;
        return me.count;
    }
};

###
### Now some fun examples:
###

#
# Make a "inverted index" hash out of a vector that returns the index
# for each element.
#
invert = func {
    vec = arg[0];
    hash = {};
    for(i=0; i<size(vec); i = i+1) {
        hash[vec[i]] = i;
    }
    return hash;
}

#
# Use the return value of the above function to do an "index of"
# lookup on a vector
#
vecfind = func{ return invert(arg[0])[arg[1]]; }

#
# Joins its arguments with the empty string and returns a scalar.
# Note use of "~" operator to do string concatenation (Nasl's only
# funny syntax).
#
join = func { 
    s = "";
    foreach(elem; arg) { s = s ~ elem; }
    return s;
}

#
# Labeled break/continue syntax puts the label in as an extra first
# argument to the for/while/foreach.
#
doneWithInnerLoopEarly = dummyFunc;
completelyDone = dummyFunc;
for(OUTER; i=0; i<100; i = i+1) {
    for(j=0; j<100; j = j+1) {
        if(doneWithInnerLoopEarly()) {
            break;
        } elsif(completelyDone()) {
            break OUTER;
        }
    }
}

#
# Functional programming A: All function expressions are inherently
# anonymous lambdas and can be used and evaluated mid-expression:
#
# (Note the parens around the function expression.  This is necessary
# because otherwise the parser would read a func following an
# assignment as a "code block" instead of a subexpression.  This is
# the rule that allows you to omit the semicolon at the end of a
# normal function definition.  Oh well, every language has a syntactic
# quirk or two...)
#
a = (func{ arg[0] + 1 })(232);  # "a" now equals 233

#
# Functional programming B.  All expressions have a value, the last
# expression in a code block is the return value of that code block.
# There are no "statements" in Nasl, although some expressions
# (assignment, duh) have side effects.  e.g. The "if" expression works
# both for code flow and as the ?: expression in C/C++.
#
factorial = func { if(arg[0] == 0) { 1 }
                   else            { arg[0] * factorial(arg[0]-1) } }
print(factorial(10), "\n");

#
# Functional programming C:  Lexical closures.
#
getcounter = func { count = 0; return func { count = count + 1 } }
mycounter = getcounter();
print(mycounter(), "\n"); # prints 1
print(mycounter(), "\n"); # prints 2
print(mycounter(), "\n"); # prints 3
print(mycounter(), "\n"); # prints 4...
