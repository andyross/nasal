# If this interpreter is not linked against the GNU readline library,
# then implement a fallback readline() implementation that still
# works.
if(!contains(caller(0)[0], "readline")) {
    import("io");
    var readline = func { io.readln(io.stdin) };
}
