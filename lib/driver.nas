# This is the top-level "driver" file containing the module import
# code for Nasal programs.  Call this file from your C code to get
# back a hash table for use in binding new functions.  You can use it
# directly, clone it to make sandboxed environments, or (in a script)
# call new_nasal_env() to create such a cloned environment even
# without access to the original hash.
#
# MODULES:
#
# Nasal modules are really simple:
#
# + Users import modules with the import function.  The first argument
#   is a string containing the module name.  Any further arguments are
#   symbols to be imported into the caller's namespace in addition to
#   the module hash.  This code:
#       import("math", "sin", "cos");
#   will import the math library as a hash table named "math" in the
#   local namespace, and also set local variables for the sin and cos
#   functions.  A single "*" as the symbol to import is shorthand for
#   "import everything into my namespace".
#
# + Module files end with ".nas" and live either in the same directory
#   as the importing file (detected dynamically), the same directory
#   as driver.nas (detected dynamically), or in the current directory,
#   in that order of preference.  (This search path could easily be
#   made to include user directories, e.g. $NASAL_LIB, in the future).
#
# + Module files are run once, the first time user code imports them.
#   The local variables defined during that script run become the
#   module namespace, with a few exceptions:
#   + Only symbols that reference "shallow" objects (not vectors or
#     hashes) are exported by default.
#   + Symbols begining with an understore (_) are not exported by default.
#   + The special "arg" and "EXPORT" symbols are not exported by default.
#
#  + Modules that want to break these rules can, by defining an EXPORT
#    vector in their local namespace containing the precicse list of
#    symbol strings to export.
#
#  + If there is a built-in module (i.e. math, regex, etc...) already
#    defined, then the symbols defined there are available as local
#    variables at the time the module script begins running.  If there
#    is no module script, then the built-in symbols are used as-is.
#
#  + Files run at the end of this driver script, or more generally
#    inside a new_nasal_env() cloned environment, are physically
#    separated from each other and do not see each other's data,
#    except where modules choose to export shared data via their
#    symbol tables or function results.
#
# SECURITY NOTES:
#
#  + If code is allowed access to bind(), caller() and closure(), it
#    will still be able to follow function references to module data.
#    Applications that want to sandbox untrusted scripts for security
#    reasons should remove these symbols from the namespace returned
#    from new_nasal_env() or replace them with wrapped versions that
#    check caller credentials (one example: limit code to examining
#    and binding to functions defined in the same file or under the
#    same directory, etc...).
#
#  + The parents reference on objects is user-visible, which breaks
#    the security encapsulation above.  Currently, this means that OOP
#    is disallowed when class objects need to be shared between
#    untrusted modules.  That may not be a bad idea, though, as
#    security-concious interfaces should be thin and minimal.  Clever
#    implementations can still provide OOP interfaces, but they must
#    do it without referencing module data in the parents array.

# Note: this implementation is currently unix-specific (including
# cygwin, of course), but needn't be with some portability work to
# getcwd() and the directory separator.

# Converts a file path, which may be relative, into a fully qualified
# directory name of the enclosing directory.  Example:
# dirname("/usr/bin/nasal") == "/usr/bin"
var dirname = func(path) {
    var lastslash = -1;
    for(var i=size(path)-1; i>=0; i-=1) {
	if(path[i] != `/`) continue;
	lastslash = i;
	break;
    }
    if(lastslash < 0) path = "";
    elsif(lastslash > 0) path = substr(path, 0, lastslash);
    if(!size(path) or path[0] != `/`) path = unix.getcwd() ~ "/" ~ path;
    return path;
}

var hashdup = func(h) {
    var result = {};
    foreach(k; keys(h))	result[k] = h[k];
    return result;
}

# Duplicated from the io library, which we can't import here:
var readfile = func(file) {
    var sz = io.stat(file)[7];
    var buf = bits.buf(sz);
    io.read(io.open(file), buf, sz);
    return buf;
}

# Generates and returns a "bare" Nasal environment, with only the
# symbols available at startup.
var new_nasal_env = func { hashdup(core_env) }

# Read and runs a file in a new_nasal_env() environment, handling
# errors and generating a stack trace.
var run_file = func(file, syms, args=nil) {
    var code = call(func { compile(readfile(file), file) }, nil, var err=[]);
    if(size(err)) die(sprintf("%s in %s", err[0], file));
    code = bind(code, new_nasal_env(), nil);
    call(code, args, nil, syms, err = []);
    if(size(err) and err[0] != "exit") {
	io.write(io.stderr, sprintf("Runtime error: %s\n", err[0]));
	for(var i=1; i<size(err); i+=2)
	    io.write(io.stderr,
		     sprintf("  %s %s line %d\n", i==1 ? "at" : "called from",
			     err[i], err[i+1]));
    }
}

# Table of loaded modules indexed by file name
var loaded_modules = {};

var find_mod = func(name, localdir) {
    var paths = [localdir] ~ module_dirs;
    forindex(i; paths) paths[i] = sprintf("%s/%s.nas", paths[i], name);
    foreach(path; paths) {
	if(loaded_modules[path] != nil) return loaded_modules[path];
    }
    foreach(path; paths) {
	if(io.stat(path) != nil) {
	    var mod = core_modules[name] == nil ? {} : core_modules[name];
	    run_file(path, mod);
	    loaded_modules[path] = mod;
	    return mod;
	}
    }
    return core_modules[name]; # Last chance: unaugmented core or nil
}

# Duplicates a module symbol table, filtering for exported symbols only
var clone_exports = func(mod) {
    var result = {};
    if(typeof(mod["EXPORT"]) == "vector") {
	foreach(sym; mod["EXPORT"])
	    result[sym] = mod[sym];
    } else {
	foreach(sym; keys(mod)) {
	    if(sym == "EXPORT" or sym == "arg") continue;
	    var t = typeof(mod[sym]);
	    if(t == "hash" or t == "vector") continue;
	    sym ~= ""; # Because in principle symbols could be numbers
	    if(size(sym) and sym[0] == `_`) continue;
	    result[sym] = mod[sym];
	}
    }
    return result;
}

var import = func(modname, imports...) {
    var mod = find_mod(modname, dirname(caller()[2]));
    if(mod == nil) die(sprintf("Cannot find module '%s'", modname));
    mod = clone_exports(mod);
    var caller_locals = caller()[0];
    caller_locals[modname] = mod;
    if(size(imports)) {
	if(imports[0] == "*") { imports = keys(mod); }
	foreach(sym; imports) {
	    if(!contains(mod, sym))
		die(sprintf("No symbol '%s' in module '%s'", sym, mod));
	    caller_locals[sym] = mod[sym];
	}
    }
    return mod;
}

# The default module loading path
# FIXME: read a NASAL_LIB_PATH environment variable or whatnot
var module_dirs = [dirname(caller(0)[2]), "."];

# Tables of "core" (built-in) functions and modules available, and a
# table of "loaded" modules that have been imported at least once.
var core_env = {};
var core_modules = {};

# Assume our "outer scope" is the hash returned from naStdLib() and
# grab the symbols therein to make our "core" environment tables.
var outer_scope = closure(caller(0)[1]);
foreach(x; keys(outer_scope)) {
    var t = typeof(outer_scope[x]);
    if(t == "func") { core_env[x] = outer_scope[x]; }
    elsif(t == "hash") { core_modules[x] = outer_scope[x]; }
}

# Add import() and new_nasal_env().
core_env["import"] = import;
core_env["new_nasal_env"] = new_nasal_env;

# Execute our command line if we have one; either "--interactive" to
# run an interactive interpreter (the bin/nasal wrapper script passes
# this when it sees no arguments), or else a script to run and pass
# the remainder of our arguments.
if(size(arg)){
    if(arg[0] == "--interactive") {
	import("interactive");
	interactive.run();
    } else {
	run_file(arg[0], {}, subvec(arg, 1));
    }
}

# Finally, our return value is a properly initialized environment that
# can be used to bind functions against.  The nasal-bin interpreter
# ignores this value, but embedded environments use this as the seed
# from which all future environments are created.
return core_env;
