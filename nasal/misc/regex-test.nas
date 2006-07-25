import("regex");
import("foo");

if(size(arg) < 2) die("Usage: regex <pattern> <string> [subst expr]");

var r = regex.new(arg[0]);
var str = arg[1];

if(size(arg) < 3) {
    while(var n = r.match(str)) {
	for(var i=0; i<n; i+=1) {
	    var gs = substr(str, r.group_start(i), r.group_len(i));
	    print(sprintf("%d: %s / %s\n", i, r.group(i), gs));
	}
	print("--\n");
    }
    
    r.reset();
    
    while(size(var mv = r.match_groups(str))) {
	foreach(s; mv)
	    print(s, "\n");
	print("--\n");
    }
} else {
    var subexpr = arg[2];
    print("sub: ", r.sub(str, subexpr), "\n");
    print("sub_all: ", r.sub_all(str, subexpr), "\n");
}
