##
# Interprets a JSP-like syntax:
# <%! ... %>  Indicates an initialization expression, executed once, at
#             compile time, in the outer scope of the page body.
#             Variables declared here will be local to the code on the
#             page, but nowhere else, and will survive multiple
#             executions of the page.
#
# <% ... %>   Is Nasal code which will be compiled into the body inline
#             (e.g. <% foreach(a; list) { %> ... <% } %>)
#
# <%= ... %>  Is a Nasal expression, the result (!) of which will be
#             inserted into the message body (e.g. <%= "abcde" %>);
#
# <%- ... %>  "Hidden" comment (does not appear in generated text)
#
# <%@ file %> Includes the named file in the generated text, expanding
#             according to the .nhtm syntax..  Works only with files,
#             not URLs: this does not work to import arbitrary web
#             content from elsewhere on the server.
#
# Why JSP?  It's the only commonly-understood embedding syntax that
# supports all of the idioms above.
#
# The generated code runs in the same environment as a ".ns" handler,
# with access to the mod_nasal extension functions in its default
# namespace, as well as to the standard nasal modules via import().
# There is one extra function here, "include(file)", which will
# include the results of the specified NHTM file into the output in
# the same sense as the <%@ file %> syntax.
#
# Implementation details:
#
# The gen_template() function returns a callable function which
# implements the page.  Note neat trick: the page "body" code is an
# inner function of another function containing the "initialization"
# blocks.  This function is evaluated to run the init code and return
# the page body function as its result.  All the scoping works out,
# and the caller gets a single function to do what is needed.
#
# Buglet: parse errors get reported on essentially arbitrary line
# numbers.  One way to fix this might involve splitting out the init
# and body code into separate compile steps and counting line numbers
# in the literal text to re-insert them in the appropriate code.
#
import("io");

var handlers = {}; # file name -> handler functions
var times = {};    # file name -> modification timestamp

var include = func(filename) {
    # Fix up relative paths to the caller's directory
    if(filename[0] != `/`) {
	var dir = caller()[2];
	var sz = size(dir);
	var slash = 0;
	for(var i=0; i<sz; i+=1) if(dir[i] == `/`) slash = i;
	dir = slash ? substr(dir, 0, slash) : dir;
	filename = dir ~ "/" ~ filename;
    }
    var handler = find_handler(filename);
    if(handler == nil) die("Cannot include file: " ~ filename);
    return handler();
}

# Like substr(s, a, len), but trims spaces
var _trimsub = func(s, a, len) {
    var b = a + len;
    while(b > a and s[b-1] == ` `) b -= 1;
    while(a < b and s[a] == ` `) a += 1;
    return substr(s, a, b-a);
}

# FIXME: add newlines to the Nasal strings to make the reported line
# numbers match those in the input file.  That requires compiling the
# init and code blocks separately, of course.
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
	elsif(type == `-`) {} # Hidden comment, noop
	elsif(type == `@`)
	    body ~= "include(\"" ~ _trimsub(s, open+3, close-open-3) ~ "\");";
	else
            body ~= substr(s, open+2, close-open-2);
        last = close + 2;
    }
    var sym = "__nhtm" ~ last;
    syms[sym] = substr(s, last);
    body ~= "print(" ~ sym ~ ")}";
    var code = compile(init ~ body, filename);
    var env = new_nasal_env();
    env.include = include; # remember this, so generated code can use it!
    code = bind(code, env, nil);
    code = bind(code, syms, code);
    return code();
}

var find_handler = func(file) {
    var stat = io.stat(file);
    if(file == nil or stat == nil)
        return nil;
    
    var timestamp = stat[9];
    if(contains(handlers, file) and timestamp == times[file])
	return handlers[file];
    
    var handler = gen_template(io.readfile(file), file);

    handlers[file] = handler;
    times[file] = timestamp;

    return handler;
}

#
# This is the per-request handler that mod_nasal will invoke:
#
return func {
    var handler = find_handler(getcgi("SCRIPT_FILENAME"));
    if(handler == nil)
        return setstatus(404);
    
    # Set us to text/html by default
    sethdr("Content-Type", "text/html");

    return handler();
}
