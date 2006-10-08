import("_gtk"); var gtk=_gtk;
import("math");
import("cairo");

monofont = "mono 12";

w = gtk.new("GtkWindow","title","Nasal gtk test","border-width",2);
gtk.connect(w,"destroy",gtk.main_quit);

vbox = gtk.new("GtkVBox","spacing",2);
gtk.container_add(w,vbox);

box = gtk.new("GtkHBox","spacing",8,"border-width",4);

gtk.box_pack_start(vbox,box,0,0);

lbl = gtk.new("GtkLabel","label","My Label");
gtk.container_add(box,lbl);

b1 = gtk.new("GtkButton","label","Hi there","use-stock",1);
gtk.container_add(box,b1);

b2 = gtk.new("GtkButton","label","gtk-save","use-stock",1);
gtk.container_add(box,b2);

b_cb = func(wid,data) {
    gtk.set(wid,"label",data);
    print(data~" beep\n");
}

gtk.connect(b1,"clicked",b_cb,"gtk-cancel");
gtk.connect(b2,"clicked",b_cb,"gtk-open");

txt = gtk.new("GtkTextView");
buf = gtk.get(txt,"buffer");
gtk.set(buf,"text","Kymatica\nLaboratories\n");
gtk.set(txt,"height-request",100,"editable",0);
#txt.modify_font(monofont);

print = func(args...) {
    gtk.emit(txt,"move-cursor","buffer-ends",1,0);
    foreach(var s;args)
        gtk.text_view_insert(txt,s);
    gtk.text_view_scroll_to_cursor(txt);
}

sw = gtk.new("GtkScrolledWindow","hscrollbar-policy","never");
gtk.container_add(sw,txt);
gtk.box_pack_start(vbox,sw);

hist_cb = func(wid,ev,data) {
    var key = ev.keyval_name;
    if(key=="Up") {
        print("history up\n");
        return 1;
    }
    if(key=="Down") {
        print("history down\n");    
        return 1;
    }
    return 0;
}
exec_cb = func(wid,data) {
    var s = gtk.get(wid,"text");
    gtk.set(wid,"text","");
    print("> "~s~"\n");
    return 0;
}

e = gtk.new("GtkEntry");
gtk.connect(e,"activate",exec_cb);
gtk.connect(e,"key-press-event",hist_cb);
gtk.box_pack_end(vbox,e,0);
#e.modify_font(monofont);

###################### cairo demo ##################

canvas = gtk.new("GtkDrawingArea","height-request",200,"width-request",300);
gtk.box_pack_end(vbox,canvas,0);

var handle_radius = 3;
var shape = [15,5,55,50,5,60,35,20,40,30,10,10];

var draw_shape = func(cr,shape) {
    cairo.move_to(cr,shape[0],shape[1]);
    for(var i=2;i<size(shape);i+=2)
        cairo.line_to(cr,shape[i],shape[i+1]);
    cairo.close_path(cr);
    cairo.set_source_rgba(cr,0,0,1,0.2);
    cairo.fill_preserve(cr);
    cairo.set_source_rgba(cr,0,0,1,0.8);
    cairo.stroke(cr);
    
    for(var i=0;i<size(shape);i+=2) {
        cairo.rectangle(cr,
            shape[i]-handle_radius+0.5,shape[i+1]-handle_radius+0.5,
            handle_radius*2,handle_radius*2);
        cairo.set_source_rgba(cr,0,0,0,0.2);
        cairo.fill_preserve(cr);
        cairo.set_source_rgba(cr,0,0,0,0.5);
        cairo.stroke(cr);
    }
}

var moving_point = -1;

var canvas_press_cb = func(w,ev) {
    var olddist = -1;
    # find point closest to cursor    
    for(var i=0;i<size(shape);i+=2) {
        var dx = shape[i]-ev.x;
        var dy = shape[i+1]-ev.y;
        var dist = math.sqrt(dx*dx + dy*dy);
        if(dist<olddist or olddist<0) {
            olddist=dist;
            moving_point=i;
        }
    }
}

var canvas_motion_cb = func(w,ev) {
    if(moving_point<0) return 0;
    shape[moving_point]=ev.x;
    shape[moving_point+1]=ev.y;
    gtk.widget_queue_draw(w);
}

var canvas_expose = func(w) {
    var cr = gtk.widget_cairo_create(w);

    cairo.move_to(cr,5,15.5);
    cairo.line_to(cr,180,5.5);
    cairo.line_to(cr,120,30);
    cairo.line_to(cr,40,160);
    cairo.set_source_rgba(cr,1,0,1,0.3);
    cairo.set_line_width(cr,1);
    cairo.fill(cr);

    cairo.rectangle(cr,43.5,43.5,80,80);
    cairo.set_source_rgba(cr,1,1,0,0.3);
    cairo.fill(cr);

    cairo.move_to(cr,50,50);
    cairo.set_source_rgb(cr,0,0.5,0);
    cairo.set_font_size(cr,20);
    cairo.show_text(cr,"Click and drag here...");

    draw_shape(cr,shape);

    return 0;
}
gtk.set(canvas,"events",["button-press-mask","button-motion-mask"]);
gtk.connect(canvas,"expose-event",canvas_expose);
gtk.connect(canvas,"button-press-event",canvas_press_cb);
gtk.connect(canvas,"motion-notify-event",canvas_motion_cb);

###################### list demo ###############################

# create a store with some data
store = gtk.list_store_new("gchararray","gboolean");
gtk.list_store_set(store,gtk.list_store_append(store),0,"Foo",1,0);
gtk.list_store_set(store,gtk.list_store_append(store),0,"Bar",1,1);
gtk.list_store_set(store,gtk.list_store_append(store),0,"Zoo",1,0);

# create the list view
list = gtk.new("GtkTreeView","model",store);

# add the columns
col = gtk.new("GtkTreeViewColumn","title","File","expand",1);
gtk.tree_view_column_add_cell(col,gtk.new("GtkCellRendererText"),0,"text",0);
gtk.tree_view_append_column(list,col);

col = gtk.new("GtkTreeViewColumn","title","Changed");
gtk.tree_view_column_add_cell(col,gtk.new("GtkCellRendererToggle"),0,"active",1);
gtk.tree_view_append_column(list,col);

# handle selections
sel_cb = func(w) {
    var row = gtk.tree_selection_get_selected(w);
    print("Selected: ");
    if(row!=nil)
        print(gtk.tree_model_get(store,row,0),"\n");
    else
        print("(Nothing)\n");
}

gtk.connect(gtk.tree_view_get_selection(list),"changed",sel_cb);

act_cb = func(w,row,col) {
    print("Activated row ",row," at col ",gtk.get(col,"title"),"\n");
}
gtk.connect(list,"row-activated",act_cb);

#################################################################

gtk.box_pack_start(vbox,list,0);

gtk.widget_show_all(w);

gtk.emit(e,"grab-focus");

count=0;
tmr = func(data) {
    count += 1;
    print(data," : ",count," timer tick\n");
    return count<10;
}
tmrtag = gtk.timeout_add(500,tmr,"XXX");

gtk.main();
