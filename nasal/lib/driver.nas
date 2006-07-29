# This is the top-level "driver" file containing the module import
# code for Nasal programs.  Nasal modules are really simple:
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

# Construct a valid path string for the directory containing this file
var dirname = func(path) {
    var lastslash = 0;
    for(var i=0; i<size(path); i+=1)
	if(path[i] == `/`)
	    lastslash = i;
    path = substr(path, 0, lastslash);
    if(!lastslash or path[0] != `/`)
	path = unix.getcwd() ~ "/" ~ path;
    return path;
}

# Clones a hash table, but only "safe" symbols: nothing prefixed with
# an underscore, not the arg or EXPORT vectors, and nothing with
# deeper references.  Modules that want to export "unsafe" things need
# to use the EXPORT facility.
var clone_syms = func(h) {
    var result = {};
    foreach(k; keys(h)) {
	if(typeof(k) != "scalar" or size(k) == 0) continue;
	if(k[0] == `_` or k == "arg" or k == "EXPORT") continue;
	if(typeof(h[k]) == "hash") continue;
	if(typeof(h[k]) == "vector") continue;
	result[k] = h[k];
    }
    return result;
}

var readfile = func(file) {
    var sz = io.stat(file)[7];
    var buf = bits.buf(sz);
    io.read(io.open(file), buf, sz);
    return buf;
}

var new_nasal_env = func { clone_syms(core_funcs) }

# Reads and runs a file in a cloned version of the standard library
var run_file = func(file, syms=nil, args=nil) {
    var err = [];
    var compfn = func { compile(readfile(file), file); };
    var code = call(compfn, nil, nil, nil, err);
    if(size(err))
	die(sprintf("%s in %s", err[0], file));
    code = bind(code, new_nasal_env(), nil);
    call(code, args, nil, syms, err);
    if(size(err) and err[0] != "exit") {
	io.write(io.stderr, sprintf("Runtime error: %s\n", err[0]));
	for(var i=1; i<size(err); i+=2)
	    io.write(io.stderr,
		     sprintf("  %s %s line %d\n", i==1 ? "at" : "called from",
			     err[i], err[i+1]));
    }
}

# Locates a module file, runs and loads it
var load_mod = func(mod, prefdir) {
    var file = nil;
    var check = prefdir ~ "/" ~ mod ~ ".nas";
    if(io.stat(check) != nil) {
	file = check;
    } else {
	foreach(dir; module_path) {
	    check = dir ~ "/" ~ mod ~ ".nas";
	    if(io.stat(check) != nil) {
		file = check;
		break;
	    }
	}
    }
    var iscore = contains(core_modules, mod);
    if(file == nil and !iscore) die("cannot find module: " ~ mod);
    var syms = iscore ? clone_syms(core_modules[mod]) : {};
    if(file != nil) run_file(file, syms);
    if(contains(syms, "EXPORT") and typeof(syms["EXPORT"]) == "vector") {
	var exports = syms["EXPORT"];
	var syms2 = {};
	foreach(s; exports)
	    if(contains(syms, s))
		syms2[s] = syms[s];
	syms = syms2;
    }
    loaded_modules[mod] = syms;
}

# This is the function exposed to users.
var import = func(mod, imports...) {
    if(!contains(loaded_modules, mod)) {
	var callerfile = caller()[2];
	load_mod(mod, dirname(callerfile));
    }
    var caller_locals = caller()[0];
    var module = clone_syms(loaded_modules[mod]);
    caller_locals[mod] = module;
    if(size(imports) == 1 and imports[0] == "*") {
	foreach(sym; keys(module)) caller_locals[sym] = module[sym];
    } else {
	foreach(sym; imports) {
	    if(contains(module, sym)) caller_locals[sym] = module[sym];
	    else die(sprintf("No symbol '%s' in module '%s'", sym, mod));
	}
    }
}

# The default module loading path
# FIXME: read a NASAL_LIB_PATH environment variable or whatnot
var module_path = [dirname(caller(0)[2]), "."];

# Tables of "core" (built-in) functions and modules available, and a
# table of "loaded" modules that have been imported at least once.
var core_funcs = {};
var core_modules = {};
var loaded_modules = {};

# Assuming our "outer scope" is indeed the naStdLib() hash, grab the
# symbols therein.
var outer_scope = closure(caller(0)[1]);
foreach(x; keys(outer_scope)) {
    var t = typeof(outer_scope[x]);
    if(t == "func") { core_funcs[x] = outer_scope[x]; }
    elsif(t == "hash") { core_modules[x] = outer_scope[x]; }
}

# Add import() and new_nasal_env().
core_funcs["import"] = import;
core_funcs["new_nasal_env"] = new_nasal_env;

# Finally, run a Nasal file that we find on the command line
if(size(arg)) {
    return run_file(arg[0], {}, subvec(arg, 1));
} else {
    # No file?   Then start an interactive session.
    import("interactive");
    interactive.run();
}
