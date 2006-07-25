import("bits");
import("io");

var _try = func(f) {
    ex = [];
    call(f, [], ex);
    return size(ex) ? ex[0] : nil;
}

var _splitcmd = func(s) {
    var i=0; var n=size(s);
    cmd = [];
    while(i<n) {
        var start = i;
        while(i<n and s[i] != ` `) { i += 1; }
        append(cmd, substr(s, start, i-start));
        while(i<n and s[i] == ` `) { i += 1; }
    }
    return cmd;
}

# FIXME: all the copying here could plausibly be a performance
# problem.  Consider wrapping libc's getenv() instead.
var getenv = func(v) {
    prefix = v ~ "=";
    foreach(e; environ()) {
        if(find(prefix, e) == 0) { return substr(e, size(v)+1); }
    }
}

var _pathfind = func(f) {
    if(find("/", f) >= 0) { return f; }
    var path = getenv("PATH");
    foreach(d; split(":", path)) {
        prog = d ~ "/" ~ f;
        if(io.stat(prog) != nil) { return prog; }
    }
}

var _parsecmd = func(argv) {
    if(size(argv) != 1) { return argv; }
    return _splitcmd(argv[0]);
}

var _run = func(cmdv, fin=nil, fout=nil, closeinchild...)
{
    if((var prog = _pathfind(cmdv[0])) == nil) {
        die(cmdv[0] ~ ": command not found");
    }
    if((var pid = fork()) == 0) {
        if(fin != nil)  { dup2(fin, io.stdin); io.close(fin); }
        if(fout != nil) { dup2(fout, io.stdout); io.close(fout); }
        foreach(f; closeinchild) { print("__CLOSE__ !\n"); io.close(f); print("__CLOSED__!\n"); }
        exec(prog, cmdv, environ());
    } else {
        if(fin != nil)  { io.close(fin); }
        if(fout != nil) { io.close(fout); }
        return pid;
    }
}

# Runs a subcommand and returns its exit code
run = func {
    var pid = _run(_parsecmd(arg));
    var code = waitpid(pid)[1];
    if(code != 0) { die("command failure: " ~ code); }
    return code;
}

# Runs a subcommand and returns a pipe to the child's stdin
run_reader = func {
    var pipe = pipe();
    _run(_parsecmd(arg), pipe[0], nil, pipe[1]);
    return pipe[1];
}

# Runs a subcommand and returns a pipe from the child's stdout
run_writer = func {
    var pipe = pipe();
    _run(_parsecmd(arg), nil, pipe[1], pipe[0]);
    return pipe[0];
}

# Runs a subcommand and returns a string containing the child's output
run_output = func {
    var pipe = pipe();
    var pid = _run(_parsecmd(arg), nil, pipe[1], pipe[0]);
    buf = bits.buf(1024);
    output = "";
    readloop = func{
	while((var len = io.read(pipe[0], buf, 1024)) > 0)
	    output ~= substr(buf, 0, len);
    };
    call(readloop); # ignore errors, pick 
    var code = waitpid(pid)[1];
    if(code != 0) { die("command failure: " ~ code); }
    return output;
}
