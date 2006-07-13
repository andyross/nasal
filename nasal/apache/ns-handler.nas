# This is the handler for .nas files -- i.e. raw code that should be
# execute as if it were a top-level handler (i.e. it should return a
# function to be used as the request handler).  This indirection is
# useful because it allows all *.nas files to be their own handlers
# without the requirement that they be defined in httpd.conf and
# without the requirement for a server restart.

var handlers = {}; # file name -> handler functions
var times = {};    # file name -> modification timestamp

var readfile = func(file) {
    var sz = io.stat(file)[7];
    var buf = bits.buf(sz);
    io.read(io.open(file), buf, sz);
    return buf;
}

#
# This is the per-request handler:
#
return func {
    # Where is our script?  Note that while most errors here are fatal
    # configuration goofs and are signaled with die(), missing files
    # are OKish, just a "404 Not Found", not a "500 Internal Server
    # Error".
    var file = getcgi("SCRIPT_FILENAME");
    var stat = io.stat(file);
    if(file == nil or stat == nil)
	return setstatus(404);
    
    # Do we already have it, and is it unchanged?
    var timestamp = stat[9];
    if(contains(handlers, file) and timestamp == times[file])
	return handlers[file]();
    
    # Nope, compile it
    var code = compile(readfile(file), file);
    var handler = code();
    if(typeof(handler) != "func")
	die("nasal handler ", file, " did not return a function");

    # FIXME: should really re-bind the handler to a sanitized copy of
    # our namespace to prevent unwanted interaction.
    handlers[file] = handler;
    times[file] = timestamp;

    # Set us to text/html by default
    sethdr("Content-Type", "text/html");

    return handler();
}



