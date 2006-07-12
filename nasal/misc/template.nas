##
# Interprets a JSP-like syntax:
# <%! ... %> Indicates an initialization expression, executed once in
#            the outer scope of the page body.  Variables declared
#            here will be local to the code on the page, but nowhere
#            else, and will survive multiple executions of the page.
# <%= ... %> Is a Nasal expression, the results of which will be
#            inserted into the message body (e.g. <%= "abcde" %>);
# <% ... %>  Is Nasal code which will be compiled into the body inline
#            (e.g. <% foreach(a; list) { %>)
#
# Why JSP?  It's the only commonly-understood embedding syntax that
# supports all three of the idioms above.  Changing it is as simple as
# changing the literals in the code below...
#
# Returns a callable function which implements the page.  This
# function takes a single argument, which is a function (itself taking
# a string) used to generate output (e.g. print).  Inside the page's
# code, this function is available as "out()".
#
# Note neat trick: the page "body" code is an inner function of
# another function containing the "initialization" blocks.  This
# function is evaluated to run the init code and return the page body
# function as its result.  All the scoping works out, and the caller
# gets a single function to do what is needed.
#
# Buglet: parse errors get reported on essentially arbitrary linen
# numbers.  One way to fix this might involve splitting out the init
# and body code into separate compile steps and counting line numbers
# in the literal text to re-insert them in the appropriate code.
#
gen_template = func(s) {
    var syms = {}; var init = ""; var body = "func(out){";
    for(var last=0; (var open = find("<%", s, last)) >= 0; last=close+2) {
        if((var close = find("%>", s, open+2)) < 0) die("unmatched <%");
        sym = "__nhtm" ~ last;
        syms[sym] = substr(s, last, open-last);
        body ~= "out(" ~ sym ~ ");";
        var type = s[open+2];
        if(type == `!`)
            init ~= substr(s, open+3, close-open-3);
        elsif(type == `=`)
            body ~= "out(" ~ substr(s, open+3, close-open-3) ~ ");";
        else
            body ~= substr(s, open+2, close-open-2);
        last = close + 2;
    }
    var sym = "__nhtm" ~ last;
    syms[sym] = substr(s, last);
    body ~= "out(" ~ sym ~ ")}";
    return bind(compile(init ~ body, "<template>"), syms, caller()[1])();
}

########################################################################
# Sample/test code:
readfile = func(file) {
    var sz = io.stat(file)[7];
    var buf = bits.buf(sz);
    io.read(io.open(file), buf, sz);
    return buf;
};
print("Generating template\n===================\n");
var f = gen_template(readfile(arg[0]));
print("\nNow calling the page\n====================\n");
f(print);
