import("_gtk");

# Map of type names to a list of [method,function] pairs.  The method
# will become a callable Nasal method on the class.  The function is
# the name of the underlying _gtk function: it will have the "me"
# reference passed as its first argument.
var class_methods = {
    "GObject" : [["get", "get"],
		 ["set", "set"],
		 ["connect", "connect"],
		 ["emit", "emit"]],
    "GtkContainer" : [["child_get", "child_get"],
		      ["child_set", "child_set"]],
    "GtkBox" : [["pack_start", "box_pack_start"],
		["pack_end", "box_pack_end"]],
    "GtkWidget" : [["show_all", "widget_show_all"],
		   ["cairo_create", "widget_cairo_create"],
		   ["queue_draw", "widget_queue_draw"]],
    "GtkTextView" : [["insert", "text_view_insert"],
		     ["scroll_to_cursor", "text_view_scroll_to_cursor"]],
    "GtkTreeView" : [["append_column", "tree_view_append_column"],
		     ["get_selection", "tree_view_get_selection"]],
    "GtkTreeSelection" : [["get_selected", "tree_selection_get_selected"],
                          ["select", "tree_selection_select"]],
    "GtkMenuItem" : [["set_submenu", "menu_item_set_submenu"]],
    "GtkListStore" : [["append", "list_store_append"],
		      ["remove", "list_store_remove"],
		      ["set_row", "list_store_set"],
		      ["get_row", "tree_model_get"],
		      ["clear", "list_store_clear"]],
    "GtkTreeViewColumn" : [["add_cell", "cell_layout_add_cell"]],
    "GtkComboBox" : [["add_cell", "cell_layout_add_cell"]],
    "GtkComboBoxEntry" : [["add_cell", "cell_layout_add_cell"]],
    "GtkCellView" : [["add_cell", "cell_layout_add_cell"]],
    "GtkIconView" : [["add_cell", "cell_layout_add_cell"]],
};

# OOP IS-A predicate
_isa = func(obj,class) {
    if(obj == nil or !contains(obj, "parents")) return 0;
    foreach(var c; obj.parents) {
        if(c == class)     return 1;
        elsif(_isa(obj, c)) return 1;
    }
    return 0;
}

# Cache the ghost type of a GObject
var _gtype = ghosttype(_gtk.new("GObject"));
var _isgobj = func(o) { ghosttype(o) == _gtype }

# Wraps a callback with a version that converts all passed ghosts to
# objects, and unpacks returned objects into ghosts.
var _wrapcb = func(cb) {
    func {
	forindex(var i; arg)
	    if (_isgobj(arg[i])) arg[i] = _object(arg[i]);
	var result = call(cb, arg);
	return _isa(result, GObjectClass) ? result.object : result;
    }
}

# Wraps a function with a version that converts all argument objects
# to ghosts and converts returned ghosts to objects.
var _wrapfn = func(fn) {
    func {
	forindex(var i; arg) {
	    if   (typeof(arg[i]) == "func")   arg[i] = _wrapcb(arg[i]);
	    elsif(_isa(arg[i], GObjectClass)) arg[i] = arg[i].object;
	}
	var result = call(fn, arg);
	return (_isgobj(result)) ? _object(result) : result;
    }
}

# Creates a new object from a GObject ghost reference
var _object = func(ghost) {
    return { parents : [_getClass(get_type(ghost))], object : ghost };
}

# Retrieves the specified class object by name
var _getClass = func(t) {
    return contains(_classes, t) ? _classes[t] : _initClass(t);
}

# GLib symbols have '-' characters in them that must be turned into
# '_' to make them valid Nasal symbols.
var _symize = func(s) {
    var sym = s ~ "";
    for(var i=0; i<size(sym); i+=1) {
	if(sym[i] >= `A` and sym[i] <= `Z`) continue;
	if(sym[i] >= `a` and sym[i] <= `z`) continue;
	if(sym[i] >= `0` and sym[i] <= `9`) continue;
	sym[i] = `_`;
    }
    return sym;
}

# Initializes a class object with proper the parent, methods, and signals.
var _genMethod = func(fn) { return func { call(fn, [me] ~ arg) } }
var _genSignal = func(sig) { return func { call(emit, [me, sig] ~ arg) } }
var _initClass = func(type) {
    var class = {};
    if((var parent = parent_type(type)) != nil)
	class.parents = [_initClass(parent)];
    foreach(var sig; _gtk.get_signals(type))
	class[_symize(sig)] = _genSignal(sig);
    if(contains(class_methods, type))
	foreach(var pair; class_methods[type])
	    class[pair[0]] = _genMethod(_ns[pair[1]]);
    return _classes[type] = class;
}

# For all the functions in the low-level _gtk module, create a wrapper
# version here that automagically converts ghosts to/from GObject
# hashes for the benefit of the code here, which can then use them
# interchangably.  Note that a "callback" function in the argument
# list (i.e. a connect() callback) automatically gets wrapped, too!
var _ns = caller(0)[0];

foreach(sym; keys(_gtk)) {
    if(typeof(_gtk[sym]) != "func") continue;
    _ns[sym] = _wrapfn(_gtk[sym]);
}

# GtkListStore has a special constructor.  Ugh.
var ListStore_new = list_store_new;

# Create our map of type name to class object
var _classes = {};

# Finally, generate constructors and create the root GObjectClass object.
var _genConstructor = func(t) { return func { call(new, [t] ~ arg); } }
var _genConstructors = func(t) {
    if(find("Gtk", t) == 0)
	_ns[substr(t,3)] = _genConstructor(t);
    foreach(var ch; _gtk.type_children(t))
        _genConstructors(ch);
}
var GObjectClass = _genConstructors("GObject");
