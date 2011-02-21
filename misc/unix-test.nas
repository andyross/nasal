import("io");
import("regex");
import("unix", "run", "run_output", "run_reader");

# List /etc using separate arguments to /bin/ls
unix.chdir("/etc");
run("/bin/ls", "-F", "--color=tty");

# Dump the password file using a string command to be parsed
run("cat /etc/passwd");

# Do another ls, but this time parse it from a string command that
# honors PATH, get the result as a string, and print it from the script.
print(run_output("ls -F --color=tty"));

# Spawn a process to take input ("cat -" just echoes it back to
# stdout, of course), feed it some, and retrieve the return code.
p = run_reader("cat", "-");
io.write(p, "Write *this* to your pipe!\n");
io.close(p);
ch = unix.waitpid(-1);
print("result of cat: ", ch[0], ", ", ch[1], "\n");

