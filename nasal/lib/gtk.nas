# Copyright 2006, Jonatan Liljedahl
# Distributable under the GNU LGPL v.2, see COPYING for details

# High level class wrapper for GTK.
# Generates constructors for all Gtk types, which creates the
# classes with proper inheritance on demand.
# We also add some special methods to some classes here.

var prepend = func(a,b...) { b ~ a; }

var _ns = caller(0)[0];

var _object = func(o) {
    var t = get_type(o);
    _genClass(t);
    {parents:[_ns[t~"Class"]],object:o};    
}

var _genConstructors = func(t) {
    if(substr(t,0,3)=="Gtk") {
        _ns[substr(t,3)] = func(args...) {
            args = prepend(args,t);
            {parents:[_genClass(t)],object:call(new,args)};
        }
    }
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

init();

_genConstructors("GObject");

GObjectClass = {
    get: func(p) {get(me.object,p);},
    set: func(args...) {
        args = prepend(args,me.object);
        call(set,args);
    },
    connect: func(sig,cb,data=nil) {connect(me.object,sig,cb,data);},
    emit: func(sig,args...) {
        args = prepend(args,me.object,sig);
        call(emit,args);
    },
};

GtkBoxClass = {
    pack_start: func(c,e=1,f=1,p=0) {
        emit(me.object,"add",c.object);
        child_set(me.object,c.object,
            "expand",e,"fill",f,"padding",p,"pack-type","start");
    },
    pack_end: func(c,e=1,f=1,p=0) {
        emit(me.object,"add",c.object);
        child_set(me.object,c.object,
            "expand",e,"fill",f,"padding",p,"pack-type","end");
    },
};

GtkContainerClass = {
    add: func(c) {
        emit(me.object,"add",c.object);
    },
    child_get: func(child,p) {child_get(me.object,child,p);},
    child_set: func(child,args...) {
        args=prepend(args,me.object,child);
        call(child_set,args);
    },

};

GtkWidgetClass = {
    show: func {emit(me.object,"show");},
    show_all: func {widget_show_all(me.object);},
    cairo_create: func {widget_cairo_create(me.object);},
    queue_draw: func {widget_queue_draw(me.object);},
};

GtkTextViewClass = {
    insert: func(s) {text_view_insert(me.object,s);},
    scroll_to_cursor: func {text_view_scroll_to_cursor(me.object);},
};

ListStore_new = func(args...) {
    _object(call(list_store_new,args));
}
GtkListStoreClass = {
    append: func { list_store_append(me.object); },
    remove: func(row) { list_store_remove(me.object,row); },
    set_row: func(row,args...) {
        args = prepend(args,me.object,row);
        call(list_store_set,args);
    },
    get_row: func(row,col) {
        tree_model_get(me.object,row,col);
    },
    clear: func { list_store_clear(me.object); },
    #TODO: get_all(), returns the whole list as nested vectors...
};

GtkTreeViewColumnClass = {
    add_cell: func(cell,expand,args...) {
        args = prepend(args,me.object,cell.object,expand);
        call(tree_view_column_add_cell,args);
    },
};

GtkTreeViewClass = {
    append_column: func(c) {
        tree_view_append_column(me.object,c.object);
    },
    get_selection: func {
        _object(tree_view_get_selection(me.object));
    }
};

GtkTreeSelectionClass = {
    get_selected: func {
        tree_selection_get_selected(me.object);
    }
};

GtkMenuItemClass = {
    set_submenu: func(sub) {
        menu_item_set_submenu(me.object,sub.object);
    }
};
