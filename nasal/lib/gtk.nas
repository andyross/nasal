# Copyright 2006, Jonatan Liljedahl, Andrew Ross
# Distributable under the GNU LGPL v.2, see COPYING for details

# High level class wrapper for GTK.
# Generates constructors for all Gtk types, which creates the
# classes with proper inheritance on demand.
# We also add some special methods to some classes here.

import("_gtk");

var _ns = caller(0)[0];

var _object = func(o) {
    var t = get_type(o);
    _genClass(t);
    {parents:[_ns[t~"Class"]],object:o};    
}

# OOP IS-A type predicate
_isa = func(obj,class) {
    if(!contains(obj, "parents")) return 0;
    foreach(var c; obj.parents) {
        if(c == class)     return 1;
        elsif(_isa(obj, c)) return 1;
    }
    return 0;
}

var _gtype = ghosttype(_gtk.new("GObject"));
var _isgobj = func(o) { typeof(o) == "ghost" and ghosttype(o) == _gtype }

# Neat trick: for all the functions in the low-level _gtk module,
# create a wrapper version here that automagically converts ghosts
# to/from GObject hashes for the benefit of the code below, which can
# then use them interchangably.  Note that a "callback" function in
# the argument list (i.e. a connect() callback) automatically gets
# wrapped, too!
var _wrapcb = func(cb) {
    func {
	forindex(var i; arg)
	    if (_isgobj(args[i])) arg[i] = _object(arg[i]);
	var result = call(cb, arg);
	return _isa(result, GObjectClass) ? result.object : result;
    }
}
var _wrapfn = func(fn) {
    func {
	forindex(var i; arg) {
	    if   (typeof(arg[i]) == "func")   arg[i] = _wrapcb(arg[i]);
	    elsif(_isa(arg[i], GObjectClass)) arg[i] = arg[i].object;
	}
	var result = call(fn, arg);
	return _isgobj(result) ? _object(result) : result;
    }
}
foreach(sym; keys(_gtk)) {
    if(typeof(_gtk[sym]) != "func") continue;
    _ns[sym] = _wrapfn(_gtk[sym]);
}

var _genConstructors = func(t) {
    if(substr(t,0,3)=="Gtk")
	_ns[substr(t,3)] = func { call(new,[t]~arg) };
    foreach(var child;type_children(t))
        _genConstructors(child);
}

var _genClass = func(t) {
    var classname = t~"Class";
    if(!contains(_ns,classname))
        _ns[classname] = {};
    var class = _ns[classname];
            
    if(contains(class,"type")) {
        return class;
    } else {
        var parent = parent_type(t);
        class.type = t;
        if(parent!=nil) {
            _genClass(parent);
            class.parents = [_ns[parent~"Class"]];
        }
    }
    return class;
}

GObjectClass = {
    get:     func(p)               { get(me,p) },
    set:     func(args...)         { call(set,[me]~args) },
    connect: func(sig,cb,data=nil) { connect(me,sig,cb,data) },
    emit:    func(sig,args...)     { call(emit,[me,sig]~args) },
};

GtkBoxClass = {
    pack_start: func(c,e=1,f=1,p=0) { box_pack_start(me,c,e,f,p); },
    pack_end:   func(c,e=1,f=1,p=0) { box_pack_end(me,c,e,f,p); },
};

GtkContainerClass = {
    add: func(c) { emit(me,"add",c); },
    child_get: func(child,p) { child_get(me,child,p) },
    child_set: func(child,args...) { call(child_set, [me,child] ~ args) },
};

GtkWidgetClass = {
    show: func {emit(me,"show");},
    show_all: func {widget_show_all(me);},
    cairo_create: func {widget_cairo_create(me);},
    queue_draw: func {widget_queue_draw(me);},
};

GtkTextViewClass = {
    insert: func(s) {text_view_insert(me,s);},
    scroll_to_cursor: func {text_view_scroll_to_cursor(me);},
};

ListStore_new = func(args...) {
    call(list_store_new,args);
}
GtkListStoreClass = {
    append:  func              { list_store_append(me); },
    remove:  func(row)         { list_store_remove(me,row); },
    set_row: func(row,args...) { call(list_store_set, [me,row] ~ args) },
    get_row: func(row,col)     { tree_model_get(me,row,col) },
    clear:   func              { list_store_clear(me); },
    #TODO: get_all(), returns the whole list as nested vectors...
};

GtkTreeViewColumnClass = {
    add_cell: func(cell,expand,args...) {
        call(tree_view_column_add_cell,[me,cell,expand] ~ args);
    },
};

GtkTreeViewClass = {
    append_column: func(c) { tree_view_append_column(me,c) },
    get_selection: func { tree_view_get_selection(me) }
};

GtkTreeSelectionClass = {
    get_selected: func { tree_selection_get_selected(me) }
};

GtkMenuItemClass = {
    set_submenu: func(sub) { menu_item_set_submenu(me,sub) }
};

_genConstructors("GObject");
