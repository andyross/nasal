##
# Generates a callable function object from an interpolation string.
# Takes a function object as the lexical environment in which the
# interpolated expressions should be evaluated, in almost all cases
# (except interpolate() below) this will be the caller's environment
# and can/should be ignored.
#
interpolater = func(s, lexenv=nil) {
    var elems = interparse(s);
    var syms = {};
    var expr = "";
    if(lexenv==nil) { var f = caller(); lexenv = bind(func{}, f[0], f[1]); }
    for(var i=0; i<size(elems); i+=1) {
        var sym = "__intersym" ~ i;
        syms[sym] = elems[i];
        expr ~= (i == 0 ? "" : "~") ~ sym;
        if((i+=1) < size(elems)) {
            expr ~= "~(" ~ elems[i] ~ ")";
        }
    }
    return bind(compile(expr), syms, lexenv);
}

##
# Uses an interpolater to evaluate a string in the lexical environment of
# the calling function.
#
interpolate = func(s) {
    var frame = caller();
    var ip = interpolater(s, bind(func{}, frame[0], frame[1]));
    return ip();
}

# Returns true if the character is legal in a symbol name.  Note that
# this allows symbols to start with numbers, which is not a legal
# symbol name in Nasal.  However, other idioms (regex substitutions)
# typically allow symbols like $1, so we do here, too.
symchr = func(c) {
    if(c >= 48 and c <= 57)  { return 1; } # 0-9
    if(c >= 65 and c <= 90)  { return 1; } # A-Z
    if(c >= 97 and c <= 122) { return 1; } # a-z
    if(c == 95) { return 1; }              # _
    return 0;
}
    
# Splits a string into a list of literal strings interleaved with nasal
# expressions.  Even indices (0, 2, ...) are always (possibly zero-length)
# strings.  Odd indices are always nasal expressions.  Does not handle
# escaping of $ characters, so you currently need to do ${'$'} if the $ is
# followed by a '{' or a symbol character ([a-zA-Z0-9_]).
interparse = func(s) {
    var str0 = 0;
    var list = [];
    for(var i=0; i<size(s)-1; i+=1) {
        if(strc(s, i) == strc("$")) {
            if(strc(s, i+1) == strc("{")) {
                var count = 0;
                var open = i+1;
                var close = -1;
                for(var j=i+2; j<size(s); j+=1) {
                    if(strc(s, j) == strc("{")) {
                        count += 1;
                    } elsif(strc(s, j) == strc("}")) {
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
                for(var j=i+1; j<size(s) and symchr(strc(s, j)); j+=1) {}
                if(j - i > 1) {
                    append(list, substr(s, str0, i-str0));
                    append(list, substr(s, i+1, j-i-1));
                    str0 = j;
                    i = j-1;
                }
            }
        }
    }
    if(str0 < size(s)) { append(list, substr(s, str0)); }
    return list;
}

########################################################################

a=1;
b=2;
sym42 = "blah blah blah";
print(interpolate("a ${a?42:'foo'} b $b, $sym42\n"));
a = 0;
print(interpolate("a ${a?42:'foo'} b $b, $sym42\n"));

ip = interpolater("a = $a\n");
print(ip());
a=2;
print(ip());
a=3;
print(ip());
a=4;
