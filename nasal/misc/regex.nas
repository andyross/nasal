# FIXME: need identity predicate to test strings w/o doing a full compare
ideq = func(a, b) { a == b }

# Usage: new(pattern, substitute expr?, options?)
# Ex: new('(\w)', '${lookup(g1)}', 'i');
# Note neat trick to use the package object (outer scope of the function)
# as the object class without needing to know its name.
new = func(pat, subopts...) {
    var opts = ""; var subexpr = nil;
    if(size(subopts) == 1) { opts = subopts[0]; }
    if(size(subopts) == 2) { opts = subopts[1]; subexpr = subopts[0]; }
    return { _re : compile(pat, opts), parents : [closure(caller(0)[0], 0)],
             str : nil, expr : subexpr ? _subex(subexpr) : nil }
}

reset = func {
    me.str = me.expr = me.result = nil;
}

match = func(str) {
    var start = 0;
    if(ideq(me.str, str)) { start = me.result[1]; } else { me.str = str; }
    me.result = exec(me._g, str, start);
    return size(me.result)/2;
}

group = func(n) {
    var start = me.result[2*n];
    return susbstr(me.str, start, me.result[2*n+1] - start);
}

sub = func(str, expr) {
    var ng = me.match(str);
    if(ng) {
        var ns = {};
        for(var i=1; i<ng; i+=1) { ns[""~i] = ns["g"~i] = me.group(i); }
        ip = interpolater(expr);
    }
}

# Compiles and returns an interpolator which runs in the caller's
# caller's (e.g. usercode -> sub() -> _subex()) namespace.
_subex = func(expr) {
    var frame = caller(2);
    return interpolater(expr, bind(func{}, frame[0], frame[1]));
}

suball = func() {
    # ...
}
