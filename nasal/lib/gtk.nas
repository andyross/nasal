# High level class wrapper for GTK.
# Generates constructors for all Gtk types, which creates the
# classes with proper inheritance on demand.
# We also add some special methods to some classes here.

var _ns = caller(0)[0];

var _object = func(o) {
    var t = get_type(o);
    _genClass(t);
    {parents:[_ns[t~"Class"]],object:o};    
}

var _genConstructors = func(t) {
    if(substr(t,0,3)=="Gtk") {
        _ns[substr(t,3)] = func(props...) {
            {parents:[_genClass(t)],object:new(t,props)};
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
    set: func(props...) {set(me.object,props);},
    connect: func(sig,cb,data=nil) {connect(me.object,sig,cb,data);},
    emit: func(sig,args...) {emit(me.object,sig,args);},
};

GtkBoxClass = {
    pack_start: func(c,e=1,f=1,p=0) {
        emit(me.object,"add",[c.object]);
        child_set(me.object,c.object,
            ["expand",e,"fill",f,"padding",p,"pack-type","start"]);
    },
    pack_end: func(c,e=1,f=1,p=0) {
        emit(me.object,"add",[c.object]);
        child_set(me.object,c.object,
            ["expand",e,"fill",f,"padding",p,"pack-type","end"]);
    },
};

GtkContainerClass = {
    add: func(c) {
        emit(me.object,"add",[c.object]);
    },
};

GtkWidgetClass = {
    show: func {emit(me.object,"show");},
    show_all: func {show_all(me.object);},
    modify_font: func(font) {widget_modify_font(me.object,font);},
    cairo_create: func {cairo_create(me.object);},
    queue_draw: func {queue_draw(me.object);},
};

GtkTextViewClass = {
    insert: func(s) {text_insert(me.object,s);},
    scroll_to_cursor: func {text_scroll_to_cursor(me.object);},
};

ListStore_new = func(args...) {
    _object(list_store_new(args));
}
GtkListStoreClass = {
    append: func { list_store_append(me.object); },
    remove: func(row) { list_store_remove(me.object,row); },
    set_row: func(row,args...) {
        list_store_set(me.object,row,args);
    },
    get_row: func(row,col) {
        tree_model_get(me.object,row,col);
    },
    clear: func { list_store_clear(me.object); },
    #TODO: get_all(), returns the whole list as nested vectors...
};

GtkTreeViewColumnClass = {
    add_cell: func(cell,expand,attrs...) {
        column_add_cell(me.object,cell.object,expand,attrs);
    },
};

GtkTreeViewClass = {
    append_column: func(c) {
        append_column(me.object,c.object);
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

