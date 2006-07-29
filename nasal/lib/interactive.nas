import("readline");
import("io");
 
var _dump = func(o, ttl=16) {
    var ot = typeof(o);
    if(ot == "scalar") { return num(o)==nil ? sprintf('"%s"', o) : o; }
    elsif(ot == "nil") { return "nil"; }
    elsif(ot == "vector" and ttl >= 0) {
        var result = "[ ";
	forindex(i; o)
	    result ~= (i==0 ? "" : ", ") ~ _dump(o[i], ttl-1);
        return result ~ " ]";
    } elsif(ot == "hash" and ttl >= 0) {
        var ks = keys(o);
        var result = "{ ";
        forindex(i; ks)
            result ~= (i==0?"":", ") ~ ks[i] ~ " : " ~ _dump(o[ks[i]], ttl-1);
        return result ~ " }";
    } else return sprintf("<%s>", ot)
}

var run = func {
    var env = new_nasal_env();
    var locals = {};
    
    while(nil != (var line = readline.readline())) {
	var err = [];
	var code = call(func { compile(line, "<input>") }, nil, nil, nil, err);
	if(size(err)) {
	    print(err[0], "\n");
	    continue;
	}
	code = bind(code, env, nil);
	var result = call(code, arg, nil, locals, err);
	if(size(err)) {
	    print(sprintf("%s at %s line %d\n", err[0], err[1], err[2]));
	    for(var i=3; i<size(err); i+=1)
		print(sprintf("  called from %s line %d\n", err[i], err[i+1]));
	    continue;
	}
	print(_dump(result), "\n");
    }
    print("\n");
}
