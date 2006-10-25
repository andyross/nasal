# A utilty class encapsulating a single regular expression
var Regex = {
    # Create a new regular expression
    new : func(pattern, opts="") {
	return { _re : comp(pattern, opts), _matches : nil, _next : 0,
		 parents : [Regex]  }
    },

    # Clear previous match information
    reset : func { me._str = nil; me._matches = nil; me._next = 0; },

    # Runs the string through the regex, returns non-zero for a
    # successful match.  Call repeatedly with the same (or nil) str
    # argument to do continued matching from the end of the last match
    # (as for perl m//g expressions).
    match : func(str=nil, start=0) {
	if(!start and me._next > 0 and (str == nil or me._str == str))
	    start = me._next;
	me._str = str;
	me._matches = exec(me._re, me._str, start);
	if(size(me._matches)) { me._next = me._matches[1]; return 1; }
	return 0;
    },

    # Returns the index after the last successful match
    next : func { me._next },

    # As match, but returns a list of zero or more substrings
    # corresponding to the matched groups.  More useful as an
    # equivalent to perl for parsing out data, e.g.:
    #     while(size(var matches = r.match_groups())) { ... }
    match_groups : func(str) {
	var result = [];
	var n = me.match(str);
	for(var i=1; i<n; i+=1)
	    append(result, me.group(i));
	return result;
    },

    # Return the specified matching group.  Group zero is the entire
    # matching substring, groups one and higher correspond to
    # parenthesized submatches.
    group : func(n) {
	var start = me._matches[2*n];
	var afterend = me._matches[2*n+1];
	return substr(me._str, start, afterend-start);
    },

    # Start index in the input string of the specified group match
    group_start : func(n) { return me._matches[2*n]; },

    # Returns the index of the character after (!) the specified group
    group_end : func(n) { return me._matches[2*n+1]; },

    # Length of the specified group match
    group_len : func(n) { return me._matches[2*n+1] - me._matches[2*n]; },
};

# Substitutes the first match of the regex object in the string with
# the evaluated expression.  As with perl, the expression can contain
# $1-$9 and $variable references.  Arbitrary nasal code can be
# included inside {} delimeters.  The group matches are available as
# both $1-$9 (which are non-standard Nasal symbols), and as _1-_9, for
# use in subexpressions, for example to decode %xx URL hex strings:
#     r = regex.new('%([0-9a-fA-F]{2})');
#     r.sub(line, "${ chr(compile("0x" ~ _1)()) }");
Regex.sub = func(str, expr, gflag=0) {
    var start = 0;
    var result = "";
    
    # Create an interpolator, and bind it to see our own "groupsyms"
    # hash table first, then the namespace of the calling function.
    var groupsyms = {};
    var lexenv = bind(func{}, caller()[0], caller()[1]);
    lexenv = bind(func{}, groupsyms, lexenv);
    var iexpr = interpolater(expr, lexenv);

    var suffix = str;
    while(size(var ov = exec(me._re, str, start))) {
	for(var gs=1; 2*gs<size(ov); gs+=1) {
	    var gi = 2*gs;
	    var g = substr(str, ov[gi], ov[gi+1]-ov[gi]);
	    groupsyms["_"~gs] = g;
	}
	var prefix = substr(str, start, ov[0]-start);
	var subst = iexpr();
	var suffix = substr(str, ov[1]);
	result ~= prefix ~ subst;
	if(!gflag) break;
	start = ov[1];
    }
    return result ~ suffix;
}

# Synonym, so users don't have to write regex.Regex.new
var new = Regex.new;

# Convenience wrapper
var match   = func(re, s, opts="")      { Regex.new(re, opts).match(s) }

# Generates a callable function object from an interpolation
# string. Takes a function object as the lexical environment in which
# the interpolated expressions should be evaluated, in almost all
# cases (except interp(), below) this will be the caller's environment
# and can/should be ignored.  FIXME: this uses a bunch of
# concatenation ops, which are O(N^2) in the number of expressions.  A
# join()-ish implementation would be better.
var interpolater = func(s, lexenv=nil) {
    var elems = _interparse(s);
    var syms = {};
    var expr = "";
    if(lexenv==nil) { var f = caller(); lexenv = bind(func{}, f[0], f[1]); }
    for(var i=0; i<size(elems); i+=1) {
        var sym = "__intersym" ~ i;
        syms[sym] = elems[i];
        expr ~= (i == 0 ? "" : "~") ~ sym;
        if((i+=1) < size(elems)) {
	    # regex hack: $1 - $9 get transmuted to $_x to be legal symbols.
	    var eval = elems[i];
	    if(size(eval) == 1 and eval[0] >= `1` and eval[0] <= `9`)
		eval = "_" ~ eval;
            expr ~= "~(" ~ eval ~ ")";
	}
    }
    return bind(compile(expr), syms, lexenv);
}

# Uses an interpolater to evaluate a string in the lexical environment of
# the calling function, e.g.: regex.interp("The value of V is: $V");
# This is basically the same as doing 'regex.interpolater("...")()',
# but the lexenv argument can be confusing for those who want to
# abstract away the extra function call, so this convenience is here
# to avoid the inevitable questions.
var interp = func(s) {
    var frame = caller();
    var ip = interpolater(s, bind(func{}, frame[0], frame[1]));
    return ip();
}

# Returns true if the character is legal in a symbol name.  Note that
# this allows symbols to start with numbers, which is not a legal
# symbol name in Nasal.  However, other idioms (regex substitutions)
# typically allow symbols like $1, so we do here, too.
var _symchr = func(c) {
    if(c >= `0` and c <= `9`) return 1;
    if(c >= `A` and c <= `Z`) return 1;
    if(c >= `a` and c <= `z`) return 1;
    if(c == `_`) return 1;
    return 0;
}
    
# Splits a string into a list: even indices (0, 2, ...) are always
# (possibly zero-length) strings, odd indices are always nasal
# expressions.
var _interparse = func(s) {
    var str0 = 0;
    var list = [];
    for(var i=0; i<size(s)-1; i+=1) {
        if(s[i] == `$`) { #` <-- perl-mode close quote
            if(s[i+1] == `$`) { #` <--- and again
	       append(list, substr(s, str0, i-str0));
	       append(list, "'$'"); # The "expression" is a literal string
	       str0 = i+2;
	       i += 1;
	   } elsif(s[i+1] == `{`) {
                var count = 0;
                var open = i+1;
                var close = -1;
                for(var j=i+2; j<size(s); j+=1) {
                    if(s[j] == `{`) {
                        count += 1;
                    } elsif(s[j] == `}`) {
                        if(count == 0) { close = j; break; }
                        count -= 1;
                    }
                }
                if(close > 0) {
                    append(list, substr(s, str0, i-str0));
                    append(list, substr(s, open+1, close-open-1));
                    str0 = close+1;
                    i = close;
                }
            } else {
                for(var j=i+1; j<size(s) and _symchr(s[j]); j+=1) {}
                if(j - i > 1) {
                    append(list, substr(s, str0, i-str0));
                    append(list, substr(s, i+1, j-i-1));
                    str0 = j;
                    i = j-1;
                }
            }
        }
    }
    if(str0 < size(s)) append(list, substr(s, str0));
    return list;
}
