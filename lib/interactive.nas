import("readline");
import("bits");
import("debug");

var _namespace = nil;

var _printf = func { print(call(sprintf, arg)) }
var _isopen = func(c) { c == `{` or c == `[` or c == `(` }
var _isclose = func(c) { c == `}` or c == `]` or c == `)` }

# Encapsulate 'static' variables to the comp func, by putting them
# inside an outer func that returns the completion function.
var _make_completer = func {
    var ns = _namespace;
    var pos = 0;
    var len = 0;
    var index = 0;
    var candidate = [];

    return func(text,state) {
        if(state==0) {
            ns = _namespace;
            pos = 0;

            var membs = split(".",text);
            if(size(membs)>1) {
                foreach(var member;membs) {
                    if(!contains(ns,member)) break;
                    ns = ns[member];
                    pos += size(member)+1;
                    if(typeof(ns)!="hash") break;
                }
            }
            
            index = 0;
            len = size(text)-pos;
            candidate = keys(ns);
        }
        while(candidate != nil and index<size(candidate)) {
            var name = candidate[index];
            index += 1;
            if(substr(text,pos,len)==substr(name,0,len)) {
                text = substr(text,0,pos) ~ name;
                if(typeof(ns[name])=="hash") { text = text ~ "."; }
                elsif(typeof(ns[name])=="func") { text = text ~ "()"; }
                return text;
            }
        }
        return nil;
    }
}

var run = func {
    var level=0;
    var in_string=0;
    var escape=0;
    var oldline="";

    _namespace = new_nasal_env();
    readline.set_completion_entry_function(_make_completer());
    
    print("Nasal interactive interpreter. Press Ctrl-D to quit.\n");

    while(CMD; 1) {
	var text = nil;
	while(LINE; 1) {
	    var s = readline.readline(level ? "... " : ">>> ");
	    if(s == nil) break CMD;
	    var len = size(s);
	    if(len == 0) continue LINE;
	    
	    for(var i=0; i<len; i+=1) {
		if(!escape) {
		    if(s[i] == `"`) {
			if   (in_string == 2) { in_string = 0; level -= 1; }
			elsif(in_string == 0) { in_string = 2; level += 1; }
		    } elsif(s[i] == `'`) {
			if   (in_string == 1) { in_string = 0; level -= 1; }
			elsif(in_string == 0) { in_string = 1; level += 1; }
		    } elsif(!in_string) {
			if   (_isopen(s[i]))  { level += 1; }
			elsif(_isclose(s[i])) { level -= 1; }
		    }
		}
		escape = (s[0] == `\\` or (!in_string and s == `\``));
	    }

	    if(s!=oldline) readline.add_history(s);
	    oldline=s;

	    text = (text == nil) ? s : text ~ "\n" ~ s;
	    
	    if(level < 0) level=0;
	    if(level != 0) continue LINE;
	    
	    var err = [];
	    var f = call(func{compile(text,"<interactive>")},nil,nil,nil,err);
	    if(size(err)) {
		print(err[0],"\n");
		continue CMD;
	    }
	    f = bind(f,_namespace,nil);
	    result = call(f,nil,nil,_namespace,err);
	    if(size(err)) {
		_printf("%s at %s line %d\n", err[0], err[1], err[2]);
		for(var i=3; i<size(err); i+=2)
		    _printf("  called from %s line %d\n", err[i], err[i+1]);
		continue CMD;
	    }
	    print(debug.dump(result),"\n");
	    text=nil;
	}
    }
}

