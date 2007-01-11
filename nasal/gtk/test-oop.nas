import("gtk");
import("math");
import("cairo");

monofont = "mono 12";

w = gtk.Window("title","Nasal gtk test","border-width",2);
w.connect("destroy",gtk.main_quit);

vbox = gtk.VBox("spacing",2);
w.add(vbox);

box = gtk.HBox("spacing",8,"border-width",4);

vbox.pack_start(box,0,0);

lbl = gtk.Label("label","My Label");
box.add(lbl);

b1 = gtk.Button("label","Hi there","use-stock",1);
box.add(b1);

b2 = gtk.Button("label","gtk-save","use-stock",1);
box.add(b2);

b_cb = func(wid,data) {
    wid.set("label",data);
    print(data~" beep\n");
}

b1.connect("clicked",b_cb,"gtk-cancel");
b2.connect("clicked",b_cb,"gtk-open");

txt = gtk.TextView();
buf = txt.get("buffer");
buf.set("text","Kymatica\nLaboratories\n");
txt.set("height-request",100,"editable",0);
#txt.modify_font(monofont);

print = func(args...) {
    txt.emit("move-cursor","buffer-ends",1,0);
    foreach(var s;args)
        txt.insert(s);
    txt.scroll_to_cursor();
}

sw = gtk.ScrolledWindow("hscrollbar-policy","never");
sw.add(txt);
vbox.pack_start(sw);

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
    var s = wid.get("text");
    wid.set("text","");
    print("> "~s~"\n");
    return 0;
}

e = gtk.Entry();
e.connect("activate",exec_cb);
e.connect("key-press-event",hist_cb);
vbox.pack_end(e,0);
#e.modify_font(monofont);

###################### cairo demo ##################

canvas = gtk.DrawingArea("height-request",200,"width-request",300);
vbox.pack_end(canvas,0);

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
    w.queue_draw();
}

var canvas_expose = func(w) {
    var cr = w.cairo_create();

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
canvas.set("events",{"button-press-mask":1,"button-motion-mask":1});
canvas.connect("expose-event",canvas_expose);
canvas.connect("button-press-event",canvas_press_cb);
canvas.connect("motion-notify-event",canvas_motion_cb);

###################### list demo ###############################

# create a store with some data
store = gtk.ListStore_new("gchararray","gboolean");
store.set_row(store.append(),0,"Foo",1,0);
store.set_row(store.append(),0,"Bar",1,1);
store.set_row(store.append(),0,"Zoo",1,0);

# create the list view
list = gtk.TreeView("model",store);

# add the columns
col = gtk.TreeViewColumn("title","File","expand",1);
col.add_cell(gtk.CellRendererText(),0,"text",0);
list.append_column(col);

col = gtk.TreeViewColumn("title","Changed");
col.add_cell(gtk.CellRendererToggle(),0,"active",1);
list.append_column(col);

# handle selections
sel_cb = func(w) {
    var row = w.get_selected();
    print("Selected: ");
    if(row!=nil)
        print(store.get_row(row,0),"\n");
    else
        print("(Nothing)\n");
}
sel = list.get_selection().connect("changed",sel_cb);

act_cb = func(w,row,col) {
    print("Activated row ",row," at col ",col.get("title"),"\n");
}
list.connect("row-activated",act_cb);

#################################################################

vbox.pack_start(list,0);

w.show_all();

e.emit("grab-focus");

count=0;
tmr = func(data) {
    count += 1;
    print(data," : ",count," timer tick\n");
    return count<10;
}
tmrtag = gtk.timeout_add(500,tmr,"XXX");

gtk.main();
