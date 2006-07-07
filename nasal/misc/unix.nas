try = func(f) {
    ex = [];
    call(f, [], ex);
    return size(ex) ? ex[0] : nil;
}

splitcmd = func(s) {
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

getenv = func(v) {
    prefix = v ~ "=";
    foreach(e; unix.environ()) {
        if(find(prefix, e) == 0) { return substr(e, size(v)+1); }
    }
}

pathfind = func(f) {
    if(find("/", f) >= 0) { return f; }
    var path = getenv("PATH");
    foreach(d; split(":", path)) {
        prog = d ~ "/" ~ f;
        if(io.stat(prog) != nil) { return prog; }
    }
}

_parsecmd = func(argv) {
    if(size(argv) != 1) { return argv; }
    return splitcmd(argv[0]);
}

_run = func(cmdv, fin=nil, fout=nil, closeinchild...)
{
    if((var prog = pathfind(cmdv[0])) == nil) {
        die(cmdv[0] ~ ": command not found");
    }
    if((var pid = unix.fork()) == 0) {
        if(fin != nil)  { unix.dup2(fin, io.stdin); io.close(fin); }
        if(fout != nil) { unix.dup2(fout, io.stdout); io.close(fout); }
        foreach(f; closeinchild) { print("__CLOSE__ !\n"); io.close(f); print("__CLOSED__!\n"); }
        unix.exec(prog, cmdv, unix.environ());
    } else {
        if(fin != nil)  { io.close(fin); }
        if(fout != nil) { io.close(fout); }
        return pid;
    }
}

# Runs a subcommand and returns its exit code
run = func {
    var pid = _run(_parsecmd(arg));
    var code = unix.waitpid(pid)[1];
    if(code != 0) { die("command failure: " ~ code); }
    return code;
}

# Runs a subcommand and returns a pipe to the child's stdin
run_reader = func {
    var pipe = unix.pipe();
    _run(_parsecmd(arg), pipe[0], nil, pipe[1]);
    return pipe[1];
}

# Runs a subcommand and returns a pipe from the child's stdout
run_writer = func {
    var pipe = unix.pipe();
    _run(_parsecmd(arg), nil, pipe[1], pipe[0]);
    return pipe[0];
}

# Runs a subcommand and returns a string containing the child's output
run_output = func {
    var pipe = unix.pipe();
    var pid = _run(_parsecmd(arg), nil, pipe[1], pipe[0]);
    buf = bits.buf(1024);
    output = "";
    try(func{
            while((var len = io.read(pipe[0], buf, 1024)) > 0) {
                output ~= substr(buf, 0, len); }});
    var code = unix.waitpid(pid)[1];
    if(code != 0) { die("command failure: " ~ code); }
    return output;
}

########################################################################

unix.chdir("/etc");
print(run("/bin/ls", "-F", "--color=tty"));
run("cat /etc/passwd");
print(run_output("ls -F --color=tty"));

p = run_reader("cat", "-");
io.write(p, "Write *this* to your pipe!\n");
io.close(p);
ch = unix.waitpid(-1);
#print("ch: ", ch[0], ", ", ch[1], "\n");
