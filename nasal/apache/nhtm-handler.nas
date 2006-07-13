var handlers = {}; # file name -> handler functions
var times = {};    # file name -> modification timestamp

var readfile = func(file) {
    var sz = io.stat(file)[7];
    var buf = bits.buf(sz);
    io.read(io.open(file), buf, sz);
    return buf;
}

# Cut-n-paste from ../misc/template.nas.  See there for details.
var gen_template = func(s, filename) {
    var syms = {}; var init = ""; var body = "func{";
    for(var last=0; (var open = find("<%", s, last)) >= 0; last=close+2) {
        if((var close = find("%>", s, open+2)) < 0) die("unmatched <%");
        sym = "__nhtm" ~ last;
        syms[sym] = substr(s, last, open-last);
        body ~= "print(" ~ sym ~ ");";
        var type = s[open+2];
        if(type == `!`)
            init ~= substr(s, open+3, close-open-3);
        elsif(type == `=`)
            body ~= "print(" ~ substr(s, open+3, close-open-3) ~ ");";
        else
            body ~= substr(s, open+2, close-open-2);
        last = close + 2;
    }
    var sym = "__nhtm" ~ last;
    syms[sym] = substr(s, last);
    body ~= "print(" ~ sym ~ ")}";
    return bind(compile(init ~ body, filename), syms, caller()[1])();
}

#
# This is the per-request handler:
#
return func {
    var file = getcgi("SCRIPT_FILENAME");
    var stat = io.stat(file);
    if(file == nil or stat == nil)
	return setstatus(404);
    
    var timestamp = stat[9];
    if(contains(handlers, file) and timestamp == times[file])
	return handlers[file]();
    
    var handler = gen_template(readfile(file), file);

    # FIXME: should really re-bind the handler to a sanitized copy of
    # our namespace to prevent unwanted interaction.
    handlers[file] = handler;
    times[file] = timestamp;

    # Set us to text/html by default
    sethdr("Content-Type", "text/html");

    return handler();
}



