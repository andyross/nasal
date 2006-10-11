/* Copyright 2006, Jonatan Liljedahl, Andrew Ross */
/* Distributable under the GNU LGPL v.2, see COPYING for details */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "nasal.h"

// From cairolib.c
naRef naNewCairoGhost(naContext ctx, cairo_t *cr);

/*
TODO:
- remove/destroy signal handler
- add all(?) gdk event types
- add methods for TreeStore.
*/

#define NASTR(b) naStr_fromdata(naNewString(ctx), (char*)(b), strlen(b))

static naRef namespace;

static void objectGhostDestroy(void* g) { g_object_unref(g); }

static naGhostType gObjectGhostType = { objectGhostDestroy, "GObject" };

static naRef timers, closures;

// Takes a reference to an existing object
static naRef object2ghost(naContext ctx, GObject *o)
{
    return o ? naNewGhost(ctx,&gObjectGhostType,g_object_ref_sink(o)) : naNil();
}

static GObject *ghost2object(naRef r)
{
    return (naGhost_type(r) == &gObjectGhostType) ? naGhost_ptr(r) : 0;
}

// Argument validation predicates
static naRef chkarg(naContext ctx, int n, int ac, naRef *av, const char* fn)
{
    if(n >= ac) naRuntimeError(ctx, "not enough arguments to gtk.%s", fn);
    return av[n];
}
static naRef strarg(naContext ctx, int n, int ac, naRef *av, const char* fn)
{
    naRef r = naStringValue(ctx, chkarg(ctx, n, ac, av, fn));
    if(naIsNil(r)) naRuntimeError(ctx, "arg %d to gtk.%s not string", n+1, fn);
    return r;
}
static naRef funcarg(naContext ctx, int n, int ac, naRef *av, const char* fn)
{
    naRef r = chkarg(ctx, n, ac, av, fn);
    if(!naIsFunc(r))
        naRuntimeError(ctx, "arg %d to gtk.%s not function", n+1, fn);
    return r;
}
static double numarg(naContext ctx, int n, int ac, naRef *av, const char* fn)
{
    naRef r = naNumValue(chkarg(ctx, n, ac, av, fn));
    if(naIsNil(r)) naRuntimeError(ctx, "arg %d to gtk.%s not number", n+1, fn);
    return r.num;
}
static void* gobjarg(naContext ctx, int n, int ac, naRef *av, const char* fn)
{
    naRef r = chkarg(ctx, n, ac, av, fn);
    if(naGhost_type(r) != &gObjectGhostType)
        naRuntimeError(ctx, "argument %d to gtk.%s not GObject", n+1, fn);
    return naGhost_ptr(r);
}

// Even simpler usage based on convention: these are to be used *only* in
// functions name f_<name>, with parameters named ctx, argc, and args.
#define STRARG(n) strarg(ctx, (n), argc, args, (__func__+2))
#define FUNCARG(n) funcarg(ctx, (n), argc, args, (__func__+2))
#define NUMARG(n) numarg(ctx, (n), argc, args, (__func__+2))
#define OBJARG(n) gobjarg(ctx, (n), argc, args, (__func__+2))

static gchar *get_stack_trace(naContext ctx)
{
    int i;
    char *buf = g_strdup_printf("%s\n  at %s, line %d\n", naGetError(ctx),
        naStr_data(naGetSourceFile(ctx, 0)), naGetLine(ctx, 0));

    for(i=1; i<naStackDepth(ctx); i++) {
        char *o = buf;
        char *s = g_strdup_printf("  called from: %s, line %d\n",
            naStr_data(naGetSourceFile(ctx, i)), naGetLine(ctx, i));
        buf = g_strconcat(buf,s,0);
        g_free(o);
        g_free(s);
    }
    return buf;
}

static guint vec2flags(naContext ctx, naRef in, GType t) {
    GFlagsClass *tc = g_type_class_ref(t);
    int i,sz = naVec_size(in);
    guint flags=0;
    if(!naIsVector(in))
        naRuntimeError(ctx,"flags not a vector of strings");
    for(i=0;i<sz;i++) {
        gchar *name = naStr_data(naStringValue(ctx,naVec_get(in,i)));
        GFlagsValue *v = g_flags_get_value_by_nick(tc,name);
        if(!v) v = g_flags_get_value_by_name(tc,name);
        if(v) flags |= v->value;
        else naRuntimeError(ctx,"Flag not found: %s",name);
    }
    g_type_class_unref(tc);
    return flags;
}

static naRef flags2vec(naContext ctx, guint state, GType type) {
    GFlagsClass *c = g_type_class_ref(type);
    naRef ret = naNewVector(ctx);
    while(state) {
        GFlagsValue *v = g_flags_get_first_value(c,state);
        naVec_append(ret,NASTR(v->value_nick));
        state -= v->value;
    }
    g_type_class_unref(c);
    return ret;
}

static naRef enum2nasal(naContext ctx, guint e, GType type) {
    GEnumClass *c = g_type_class_ref(type);
    naRef ret;
    GEnumValue *v = g_enum_get_value(c,e);
    ret = NASTR(v->value_nick);
    g_type_class_unref(c);
    return ret;
}

static naRef wrap_gdk_event(naContext ctx, GdkEvent *ev) {
#define SET_NUM(a,b) naAddSym(ctx,h,(a),naNum(b))
#define SET_STR(a,b) naAddSym(ctx,h,(a),NASTR(b))
#define SET_FLAGS(a,b,t) naAddSym(ctx,h,(a),flags2vec(ctx,b,t))

    naRef h = naNewHash(ctx);
    GEnumClass *tc = g_type_class_ref(GDK_TYPE_EVENT_TYPE);
    GEnumValue *v = g_enum_get_value(tc,ev->type);

    SET_STR("type",v->value_nick);
    g_type_class_unref(tc);

    switch(ev->type) {
        case GDK_KEY_PRESS:
        case GDK_KEY_RELEASE:
            SET_NUM("time",ev->key.time);
            SET_NUM("state",ev->key.state);
            SET_NUM("keyval",ev->key.keyval);
            SET_STR("keyval_name",gdk_keyval_name(ev->key.keyval));
            SET_NUM("hardware_keycode",ev->key.hardware_keycode);
            SET_NUM("group",ev->key.group);
//            SET_NUM("is_modifier",ev->key.is_modifier);
        break;
        case GDK_BUTTON_PRESS:
        case GDK_2BUTTON_PRESS:
        case GDK_3BUTTON_PRESS:
        case GDK_BUTTON_RELEASE:
            SET_NUM("time",ev->button.time);
            SET_NUM("x",ev->button.x);
            SET_NUM("y",ev->button.y);
            SET_NUM("x_root",ev->button.x_root);
            SET_NUM("y_root",ev->button.y_root);
            SET_FLAGS("state",ev->button.state,GDK_TYPE_MODIFIER_TYPE);
            SET_NUM("button",ev->button.button);
        break;
        case GDK_MOTION_NOTIFY:
            SET_NUM("time",ev->motion.time);
            SET_NUM("x",ev->motion.x);
            SET_NUM("y",ev->motion.y);
            SET_NUM("x_root",ev->motion.x_root);
            SET_NUM("y_root",ev->motion.y_root);
            SET_FLAGS("state",ev->button.state,GDK_TYPE_MODIFIER_TYPE);
        break;
        case GDK_EXPOSE:
            SET_NUM("count",ev->expose.count);
            SET_NUM("area_x",ev->expose.area.x);
            SET_NUM("area_y",ev->expose.area.y);
            SET_NUM("area_width",ev->expose.area.width);
            SET_NUM("area_height",ev->expose.area.height);
        break;
        case GDK_ENTER_NOTIFY:
        case GDK_LEAVE_NOTIFY:
            SET_NUM("time",ev->crossing.time);
            SET_NUM("x",ev->crossing.x);
            SET_NUM("y",ev->crossing.y);
            SET_NUM("x_root",ev->crossing.x_root);
            SET_NUM("y_root",ev->crossing.y_root);
            SET_NUM("focus",ev->crossing.focus);
            SET_FLAGS("state",ev->button.state,GDK_TYPE_MODIFIER_TYPE);
        break;
    default:
        return naNil();
    }
    return h;
#undef SET_NUM
#undef SET_STR
}

static void n2g(naContext ctx, naRef in, GValue *out)
{
#define CHECK_NUM(T,t) \
    if(G_VALUE_HOLDS_##T(out)) g_value_set_##t(out,naNumValue(in).num); else

    CHECK_NUM(BOOLEAN,boolean)
    CHECK_NUM(CHAR,char)
    CHECK_NUM(UCHAR,uchar)
    CHECK_NUM(INT,int)
    CHECK_NUM(UINT,uint)
    CHECK_NUM(LONG,long)
    CHECK_NUM(ULONG,ulong)
    CHECK_NUM(INT64,int64)
    CHECK_NUM(UINT64,uint64)
    CHECK_NUM(FLOAT,float)
    CHECK_NUM(DOUBLE,double)
    if(G_VALUE_HOLDS_ENUM(out)) {
        GEnumClass *tc = g_type_class_ref(G_VALUE_TYPE(out));
        gchar *name = naStr_data(naStringValue(ctx,in));
        GEnumValue *v = g_enum_get_value_by_nick(tc,name);
        if(!v) v = g_enum_get_value_by_name(tc,name);
        if(v) g_value_set_enum(out,v->value);
        else naRuntimeError(ctx,"Enum not found: %s",name);
        g_type_class_unref(tc);
    } else
    if(G_VALUE_HOLDS_FLAGS(out)) {
        g_value_set_flags(out,vec2flags(ctx,in,G_VALUE_TYPE(out)));
    } else
    if(G_VALUE_HOLDS_STRING(out)) {
        g_value_set_string(out,naStr_data(naStringValue(ctx,in)));
    } else
    if(G_VALUE_HOLDS_OBJECT(out)) {
        g_value_set_object(out,ghost2object(in));
    } else
        naRuntimeError(ctx,"Can't convert to type %s\n",G_VALUE_TYPE_NAME(out));
#undef CHECK_NUM
}

static naRef g2n(naContext ctx, const GValue *in)
{
#define CHECK_NUM(T,t) \
    if(G_VALUE_HOLDS_##T(in)) return naNum(g_value_get_##t(in)); else

    CHECK_NUM(BOOLEAN,boolean)
    CHECK_NUM(CHAR,char)
    CHECK_NUM(UCHAR,uchar)
    CHECK_NUM(INT,int)
    CHECK_NUM(UINT,uint)
    CHECK_NUM(LONG,long)
    CHECK_NUM(ULONG,ulong)
    CHECK_NUM(INT64,int64)
    CHECK_NUM(UINT64,uint64)
    CHECK_NUM(FLOAT,float)
    CHECK_NUM(DOUBLE,double)
    if(G_VALUE_HOLDS_STRING(in)) {
        const gchar *str = g_value_get_string(in);
        return NASTR(str);
    } else if(G_VALUE_HOLDS_OBJECT(in)) {
        return object2ghost(ctx,g_value_get_object(in));
    } else if(G_VALUE_HOLDS(in,GTK_TYPE_TREE_PATH)) {
        gchar *path = gtk_tree_path_to_string((GtkTreePath*)g_value_get_boxed(in));
        naRef ret = NASTR(path);
        g_free(path);
        return ret;
    } else if(G_VALUE_HOLDS_FLAGS(in)) {
        return flags2vec(ctx,g_value_get_flags(in),G_VALUE_TYPE(in));
    } else if(G_VALUE_HOLDS_ENUM(in)) {
        return enum2nasal(ctx,g_value_get_enum(in),G_VALUE_TYPE(in));
    } else if(G_VALUE_HOLDS(in,GDK_TYPE_EVENT)) {
        return wrap_gdk_event(ctx,g_value_get_boxed(in));
    } else {
        naRuntimeError(ctx,"Can't convert from type %s\n",G_VALUE_TYPE_NAME(in));
    }
    return naNil();
#undef CHECK_NUM
}

// Frustrating.  Initialize all known GLib types so that the runtime
// table is properly populated.  Wastes memory...
static void init_all_types()
{
    volatile GType x; // dummy, so the calls don't get optimized out
#define GT(t) (x = (gtk_##t##_get_type)())
    GT(about_dialog); GT(accel_flags); GT(accel_group); GT(accel_label);
    GT(accel_map); GT(accessible); GT(action); GT(action_group);
    GT(adjustment); GT(alignment); GT(anchor_type); GT(arg_flags); GT(arrow);
    GT(arrow_type); GT(aspect_frame); GT(attach_options); GT(bin); GT(border);
    GT(box); GT(button_action); GT(button_box); GT(button_box_style);
    GT(button); GT(buttons_type); GT(calendar_display_options); GT(calendar);
    GT(cell_editable); GT(cell_layout); GT(cell_renderer_combo);
    GT(cell_renderer); GT(cell_renderer_mode); GT(cell_renderer_pixbuf);
    GT(cell_renderer_progress); GT(cell_renderer_state);
    GT(cell_renderer_text); GT(cell_renderer_toggle); GT(cell_type);
    GT(cell_view); GT(check_button); GT(check_menu_item); GT(clipboard);
    GT(clist_drag_pos); GT(color_button);
    GT(color_selection_dialog); GT(color_selection); GT(combo_box_entry);
    GT(combo_box); GT(container); GT(corner_type);
    GT(ctree_expander_style); GT(ctree_expansion_type);
    GT(ctree_line_style); GT(ctree_pos); GT(curve);
    GT(curve_type); GT(debug_flag); GT(delete_type); GT(dest_defaults);
    GT(dialog_flags); GT(dialog); GT(direction_type); GT(drawing_area);
    GT(editable); GT(entry_completion); GT(entry); GT(event_box); GT(expander);
    GT(expander_style); GT(file_chooser_action); GT(file_chooser_button);
    GT(file_chooser_dialog); GT(file_chooser_error); GT(file_chooser);
    GT(file_chooser_widget); GT(file_filter_flags); GT(file_filter);
    GT(file_selection); GT(fixed); GT(font_button); GT(font_selection_dialog);
    GT(font_selection); GT(frame); GT(gamma_curve); GT(handle_box); GT(hbox);
    GT(hbutton_box); GT(hpaned); GT(hruler); GT(hscale); GT(hscrollbar);
    GT(hseparator); GT(icon_factory); GT(icon_info); GT(icon_lookup_flags);
    GT(icon_set); GT(icon_size); GT(icon_source); GT(icon_theme_error);
    GT(icon_theme); GT(icon_view); GT(identifier); GT(image);
    GT(image_menu_item); GT(image_type); GT(im_context); GT(im_context_simple);
    GT(im_multicontext); GT(im_preedit_style); GT(im_status_style);
    GT(input_dialog); GT(invisible); GT(item);
    GT(justification); GT(label); GT(layout);
    GT(list_store); GT(match_type); GT(menu_bar); GT(menu_direction_type);
    GT(menu); GT(menu_item); GT(menu_shell); GT(menu_tool_button);
    GT(message_dialog); GT(message_type); GT(metric_type); GT(misc);
    GT(movement_step); GT(notebook); GT(notebook_tab); GT(object_flags);
    GT(object); GT(orientation);
    GT(pack_type); GT(paned); GT(path_priority_type); GT(path_type);
    GT(plug); GT(policy_type); GT(position_type);
    GT(preview_type); GT(private_flags); GT(progress_bar);
    GT(progress_bar_orientation); GT(progress_bar_style);
    GT(radio_action); GT(radio_button); GT(radio_menu_item);
    GT(radio_tool_button); GT(range); GT(rc_flags); GT(rc_style);
    GT(rc_token_type); GT(relief_style); GT(requisition); GT(resize_mode);
    GT(response_type); GT(ruler); GT(scale); GT(scrollbar);
    GT(scrolled_window); GT(scroll_step); GT(scroll_type); GT(selection_data);
    GT(selection_mode); GT(separator); GT(separator_menu_item);
    GT(separator_tool_item); GT(settings); GT(shadow_type); GT(side_type);
    GT(signal_run_type); GT(size_group); GT(size_group_mode); GT(socket);
    GT(sort_type); GT(spin_button); GT(spin_button_update_policy);
    GT(spin_type); GT(state_type); GT(statusbar); GT(style);
    GT(submenu_direction); GT(submenu_placement); GT(table); GT(target_flags);
    GT(tearoff_menu_item); GT(text_attributes); GT(text_buffer);
    GT(text_child_anchor); GT(text_direction); GT(text_iter); GT(text_mark);
    GT(text_search_flags); GT(text_tag); GT(text_tag_table); GT(text_view);
    GT(text_window_type); GT(toggle_action); GT(toggle_button);
    GT(toggle_tool_button); GT(toolbar_child_type); GT(toolbar);
    GT(toolbar_space_style); GT(toolbar_style); GT(tool_button); GT(tool_item);
    GT(tooltips); GT(tree_drag_dest); GT(tree_drag_source); GT(tree_iter);
    GT(tree_model_filter); GT(tree_model_flags); GT(tree_model);
    GT(tree_model_sort); GT(tree_path); GT(tree_row_reference);
    GT(tree_selection); GT(tree_sortable); GT(tree_store);
    GT(tree_view_column); GT(tree_view_column_sizing);
    GT(tree_view_drop_position); GT(tree_view); GT(tree_view_mode);
    GT(ui_manager); GT(ui_manager_item_type); GT(update_type); GT(vbox);
    GT(vbutton_box); GT(viewport); GT(visibility); GT(vpaned); GT(vruler);
    GT(vscale); GT(vscrollbar); GT(vseparator); GT(widget_flags); GT(widget);
    GT(widget_help_type); GT(window); GT(window_group); GT(window_position);
    GT(window_type); GT(wrap_mode);
#undef GT
#define GT(t) (x = (gdk_##t##_get_type)())
    GT(axis_use); GT(byte_order); GT(cap_style); GT(color); GT(colormap);
    GT(colorspace); GT(crossing_mode); GT(cursor); GT(cursor_type); GT(device);
    GT(display); GT(display_manager); GT(drag_action); GT(drag_context);
    GT(drag_protocol); GT(drawable); GT(event); GT(event_mask); GT(event_type);
    GT(extension_mode); GT(fill); GT(fill_rule); GT(filter_return);
    GT(font_type); GT(function); GT(gc); GT(gc_values_mask); GT(grab_status);
    GT(gravity); GT(image); GT(image_type); GT(input_condition);
    GT(input_mode); GT(input_source); GT(interp_type); GT(join_style);
    GT(keymap); GT(line_style); GT(modifier_type); GT(notify_type);
    GT(overlap_type); GT(owner_change); GT(pango_renderer); GT(pixbuf);
    GT(pixbuf_alpha_mode); GT(pixbuf_animation); GT(pixbuf_animation_iter);
    GT(pixbuf_error); GT(pixbuf_loader); GT(pixbuf_rotation); GT(pixmap);
    GT(property_state); GT(prop_mode); GT(rectangle); GT(rgb_dither);
    GT(screen); GT(scroll_direction); GT(setting_action); GT(status);
    GT(subwindow_mode); GT(visibility_state); GT(visual); GT(visual_type);
    GT(window_attributes_type); GT(window_class); GT(window_edge);
    GT(window_hints); GT(window_object); GT(window_state); GT(window_type);
    GT(window_type_hint); GT(wm_decoration); GT(wm_function);
#undef GT
}

typedef struct {
    GClosure closure;
    naRef callback;
    naRef data;
    naRef tag;
} NasalClosure;

static naRef do_call(naContext ctx0, naRef code, int argc, naRef* args)
{
    naRef result;
    naContext ctx = ctx0 ? ctx0 : naNewContext();
    naModUnlock();
    result = naCall(ctx, code, argc, args, naNil(), naNil());
    naModLock();
    if(naGetError(ctx)) {
        gchar *trace = get_stack_trace(ctx);
        g_printerr("Nasal Error: %s", trace);
        g_free(trace);
    }
    if(!ctx0) naFreeContext(ctx);
    return result;
}

static void nasal_marshal(NasalClosure *closure, GValue *retval, guint n_parms,
                          const GValue *parms, gpointer hint, gpointer data)
{
    int i;
    naContext ctx = naNewContext();
    naRef result, *args = g_alloca(sizeof(naRef) * (n_parms+1));
    for(i=0;i<n_parms;i++)
        args[i] = g2n(ctx,&parms[i]);
    args[i]=closure->data;
    result = do_call(ctx, closure->callback, n_parms+1, args);
    if(retval && G_VALUE_TYPE(retval))
        n2g(ctx,result,retval);
    naFreeContext(ctx);
}

static void nasal_closure_finalize(gpointer notify_data, GClosure *closure)
{
    naHash_delete(closures,((NasalClosure *)closure)->tag);
}

NasalClosure *new_nasal_closure(naContext ctx, naRef callback, naRef data)
{
    GClosure *closure;
    NasalClosure *naclosure;
    closure = g_closure_new_simple(sizeof(NasalClosure), NULL);
    naclosure = (NasalClosure*)closure;
    naclosure->callback = callback;
    naclosure->data = data;
    g_closure_add_finalize_notifier(closure, NULL, nasal_closure_finalize);
    g_closure_set_marshal(closure,(GClosureMarshal)nasal_marshal);
    return naclosure;
}

static naRef f_connect(naContext ctx, naRef me, int argc, naRef* args)
{
    GObject *w = OBJARG(0);
    gchar *s = naStr_data(STRARG(1));
    naRef func = FUNCARG(2);
    naRef data = argc>3?args[3]:naNil();
    NasalClosure *closure = new_nasal_closure(ctx,func,data);
    gulong tag = g_signal_connect_closure(w,s,(GClosure*)closure,FALSE);
    naRef v = naNewVector(ctx);
    closure->tag = naNum(tag);
    naVec_append(v,func);
    naVec_append(v,data);
    naHash_set(closures,closure->tag,v);
    return naNum(tag);
}

static naRef f_new(naContext ctx, naRef me, int argc, naRef* args)
{
    GObject *obj;
    GObjectClass *klass;
    GParameter *parms;
    int i,p,n_parms = (argc-1)/2;
    naRef t_name = STRARG(0);
    GType t = g_type_from_name(naStr_data(t_name));
    if(!t) naRuntimeError(ctx,"No such type: %s",naStr_data(t_name));
    klass = G_OBJECT_CLASS(g_type_class_ref(t));
    parms = g_alloca(sizeof(GParameter) * n_parms);
    for(i=1,p=0;i<argc-1;i+=2,p++) {
        GValue gval = {0,};
        gchar *prop = naStr_data(STRARG(i));
        GParamSpec *pspec = g_object_class_find_property(klass, prop);
        if(!pspec) naRuntimeError(ctx,"No such property: %s",prop);
        g_value_init(&gval,G_PARAM_SPEC_VALUE_TYPE(pspec));
        n2g(ctx,args[i+1],&gval);
        parms[p].name = prop;
        parms[p].value = gval;
    }
    obj = g_object_newv(t,n_parms,parms);
    g_type_class_unref(klass);
    return object2ghost(ctx,obj);
}

static naRef f_set(naContext ctx, naRef me, int argc, naRef* args)
{
    GObject *obj = OBJARG(0);
    int i;
    for(i=1;i<argc-1;i+=2) {
        gchar *prop = naStr_data(STRARG(i));
        GValue gval = {0,};
        GObjectClass *class = G_OBJECT_GET_CLASS(obj);
        GParamSpec* pspec = g_object_class_find_property(class, prop);
        if(!pspec) naRuntimeError(ctx,"No such property: %s",prop);
        g_value_init(&gval,G_PARAM_SPEC_VALUE_TYPE(pspec));
        n2g(ctx,args[i+1],&gval);
        g_object_set_property(obj,prop,&gval);
    }
    return naNil();
}

static naRef f_get(naContext ctx, naRef me, int argc, naRef* args)
{
    GObject *obj = OBJARG(0);
    gchar *prop = naStr_data(STRARG(1));
    naRef nval;
    GValue gval = {0,};
    GObjectClass *class = G_OBJECT_GET_CLASS(obj);
    GParamSpec* pspec = g_object_class_find_property(class, prop);
    if(!pspec) naRuntimeError(ctx,"No such property: %s",prop);
    g_value_init(&gval,G_PARAM_SPEC_VALUE_TYPE(pspec));
    g_object_get_property(obj,prop,&gval);
    nval = g2n(ctx,&gval);
    g_value_unset(&gval);
    return nval;
}

static naRef f_child_set(naContext ctx, naRef me, int argc, naRef* args)
{
    GObject *obj = OBJARG(0);
    GObject *child = OBJARG(1);
    int i;
    for(i=2;i<argc-1;i+=2) {
        gchar *prop = naStr_data(STRARG(i));
        GValue gval = {0,};
        GObjectClass *class = G_OBJECT_GET_CLASS(obj);
        GParamSpec* pspec = gtk_container_class_find_child_property(class, prop);
        if(!pspec) naRuntimeError(ctx,"No such property: %s",prop);
        g_value_init(&gval,G_PARAM_SPEC_VALUE_TYPE(pspec));
        n2g(ctx,args[i+1],&gval);
        gtk_container_child_set_property(GTK_CONTAINER(obj),GTK_WIDGET(child),
                                         prop,&gval);
    }
    return naNil();
}

static naRef f_child_get(naContext ctx, naRef me, int argc, naRef* args)
{
    GObject *obj = OBJARG(0);
    gchar *prop = naStr_data(STRARG(2));
    naRef nval;
    GValue gval = {0,};
    GObjectClass *class = G_OBJECT_GET_CLASS(obj);
    GParamSpec* pspec = gtk_container_class_find_child_property(class, prop);
    if(!pspec) naRuntimeError(ctx,"No such property: %s",prop);
    g_value_init(&gval,G_PARAM_SPEC_VALUE_TYPE(pspec));
    gtk_container_child_get_property(GTK_CONTAINER(obj),
                                     GTK_WIDGET(OBJARG(1)),prop,&gval);
    nval = g2n(ctx,&gval);
    g_value_unset(&gval);
    return nval;
}

// This used to to be wrapped under a static variable to allow it to
// be called multiple times, but Andy couldn't understand why.
static naRef f_init(naContext ctx, naRef me, int argc, naRef* args)
{
    gtk_init(NULL,NULL);
    init_all_types();
    return naNil();
}

static naRef f_main(naContext ctx, naRef me, int argc, naRef* args)
{
    gtk_main();
    return naNil();
}

static naRef f_main_quit(naContext ctx, naRef me, int argc, naRef* args)
{
    gtk_main_quit();
    return naNil();
}

static naRef f_show_all(naContext ctx, naRef me, int argc, naRef* args)
{
    gtk_widget_show_all(OBJARG(0));
    return naNil();
}

static gboolean _timer_wrapper(long id)
{
    naRef v, result, data;
    naHash_get(timers,naNum(id),&v);
    data = naVec_get(v,1);
    result = do_call(0, naVec_get(v, 0), 1, &data);
    return (gboolean)naNumValue(result).num;
}

static void _destroy_timer(long id) { naHash_delete(timers,naNum(id)); }

static naRef f_timeout_add(naContext ctx, naRef me, int argc, naRef* args)
{
    static long id=0;
    guint interval = (guint)NUMARG(0);
    naRef func = FUNCARG(1);
    naRef data = argc>2?args[2]:naNil();
    naRef v = naNewVector(ctx);
    gulong tag = g_timeout_add_full(G_PRIORITY_DEFAULT,interval,
        (GSourceFunc)_timer_wrapper,(gpointer)id,(GDestroyNotify)_destroy_timer);

    naVec_append(v,func);
    naVec_append(v,data);
    naHash_set(timers,naNum(id),v);

    id++;
    return naNum(tag);
}

static naRef f_source_remove(naContext ctx, naRef me, int argc, naRef* args)
{
    if(argc<1) return naNil();
    g_source_remove((gulong)NUMARG(0));
    return naNil();
}

static naRef f_text_scroll_to_cursor(naContext ctx, naRef me, int argc, naRef* args)
{
    GtkTextView *o = GTK_TEXT_VIEW(OBJARG(0));
    GtkTextBuffer *buf = gtk_text_view_get_buffer(o);
    GtkTextMark *curs = gtk_text_buffer_get_insert(buf);
    gtk_text_view_scroll_mark_onscreen(o,curs);
    return naNil();
}

static naRef f_text_insert(naContext ctx, naRef me, int argc, naRef* args)
{
    GtkTextView *o = GTK_TEXT_VIEW(OBJARG(0));
    GtkTextBuffer *buf = gtk_text_view_get_buffer(o);
    gchar *s = naStr_data(STRARG(1));
    gtk_text_buffer_insert_at_cursor(buf,s,-1);
    return naNil();
}

static naRef f_queue_draw(naContext ctx, naRef me, int argc, naRef* args)
{
    gtk_widget_queue_draw(GTK_WIDGET(OBJARG(0)));
    return naNil();
}

static naRef f_emit(naContext ctx, naRef me, int argc, naRef* args)
{
    GObject *o = G_OBJECT(OBJARG(0));
    gchar *signame = naStr_data(STRARG(1));
    GType itype = G_TYPE_FROM_INSTANCE(o);
    guint sig_id = g_signal_lookup(signame,itype);
    GSignalQuery sigq;
    GValue *parms, retval={0,};
    int p,i;

    if(!sig_id) naRuntimeError(ctx,"No such signal: %s",signame);
    g_signal_query(sig_id,&sigq);

    if(argc-2!=sigq.n_params)
        naRuntimeError(ctx,"Signal %s needs %d args, got %d",signame,sigq.n_params,argc-2);

    parms = g_malloc0(sizeof(GValue)*(sigq.n_params+1));
    g_value_init(&parms[0],itype);
    g_value_set_instance(&parms[0],o);

    for(p=0,i=2;i<argc;i++,p++) {
        GValue *parm = &parms[p+1];
        g_value_init(parm,sigq.param_types[p]);
        n2g(ctx,args[i],parm);
    }

    g_signal_emitv(parms,sig_id,0,&retval);
    g_free(parms);
    if(G_VALUE_TYPE(&retval)==0)
        return naNil();
    else
        return g2n(ctx,&retval);
}

static naRef f_list_store_new(naContext ctx, naRef me, int argc, naRef* args)
{
    int i;
    GType *types = g_alloca(sizeof(GType)*argc);
    GtkListStore *w;

    for(i=0;i<argc;i++)
        types[i] = g_type_from_name(naStr_data(STRARG(i)));

    w = gtk_list_store_newv(argc,types);
    return object2ghost(ctx,G_OBJECT(w));
}


static naRef f_list_store_append(naContext ctx, naRef me, int argc, naRef* args)
{
    GtkListStore *w = GTK_LIST_STORE(OBJARG(0));
    GtkTreeIter ti;
    naRef ret;
    gchar *path;
    gtk_list_store_append(w,&ti);
    path = gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(w),&ti);
    ret = NASTR(path);
    g_free(path);
    return ret;
}

static naRef f_list_store_set(naContext ctx, naRef me, int argc, naRef* args)
{
    GtkListStore *w = GTK_LIST_STORE(OBJARG(0));
    GtkTreeIter iter;
    const gchar *path = naStr_data(STRARG(1));
    int i;

    if(!gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(w),&iter,path))
        naRuntimeError(ctx,"No such tree path: %s",path);

    for(i=2;i<argc-1;i+=2) {
        gint column = (gint)NUMARG(i);
        GType coltype = gtk_tree_model_get_column_type(GTK_TREE_MODEL(w),column);
        GValue value = {0,};

        g_value_init(&value,coltype);
        n2g(ctx,args[i+1],&value);
        gtk_list_store_set_value(w,&iter,column,&value);
    }

    return naNil();
}

static naRef f_list_store_remove(naContext ctx, naRef me, int argc, naRef* args)
{
    GtkListStore *w = GTK_LIST_STORE(OBJARG(0));
    GtkTreeIter iter;
    const gchar *path = naStr_data(STRARG(1));
    if(!gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(w),&iter,path))
        naRuntimeError(ctx,"No such tree path: %s",path);
    gtk_list_store_remove(w,&iter);
    return naNil();
}

static naRef f_list_store_clear(naContext ctx, naRef me, int argc, naRef* args)
{
    GtkListStore *w = GTK_LIST_STORE(OBJARG(0));
    gtk_list_store_clear(w);
    return naNil();
}

static naRef f_tree_model_get(naContext ctx, naRef me, int argc, naRef* args)
{
    GtkTreeModel *o = GTK_TREE_MODEL(OBJARG(0));
    GtkTreeIter iter;
    const gchar *path = naStr_data(STRARG(1));
    gint column = (gint)NUMARG(2);
    GValue value = {0,};
    naRef retval;
    if(!gtk_tree_model_get_iter_from_string(o,&iter,path))
        naRuntimeError(ctx,"No such tree path: %s",path);
    gtk_tree_model_get_value(o,&iter,column,&value);
    retval = g2n(ctx,&value);
    g_value_unset(&value);
    return retval;
}


static naRef f_column_add_cell(naContext ctx, naRef me, int argc, naRef* args)
{
    GtkTreeViewColumn *col = GTK_TREE_VIEW_COLUMN(OBJARG(0));
    GtkCellRenderer *cell = GTK_CELL_RENDERER(OBJARG(1));
    gboolean expand = (gboolean)NUMARG(2);
    int i;
    gtk_tree_view_column_pack_start(col,cell,expand);
    gtk_tree_view_column_clear_attributes(col,cell);
    for(i=3;i<argc-1;i+=2) {
        const gchar *attr = naStr_data(STRARG(i));
        gint c = (gint)NUMARG(i+1);
        gtk_tree_view_column_add_attribute(col,cell,attr,c);
    }
    return naNil();
}

static naRef f_append_column(naContext ctx, naRef me, int argc, naRef* args)
{
    GtkTreeView *w = GTK_TREE_VIEW(OBJARG(0));
    GtkTreeViewColumn *c = GTK_TREE_VIEW_COLUMN(OBJARG(1));
    gtk_tree_view_append_column(w,c);
    return naNil();
}

static naRef f_tree_view_get_selection(naContext ctx, naRef me, int argc, naRef* args)
{
    GtkTreeView *w = GTK_TREE_VIEW(OBJARG(0));
    return object2ghost(ctx,G_OBJECT(gtk_tree_view_get_selection(w)));
}

static naRef f_tree_selection_get_selected(naContext ctx, naRef me, int argc, naRef* args)
{
    GtkTreeSelection *o = GTK_TREE_SELECTION(OBJARG(0));
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *path;
    naRef ret;

    if(!gtk_tree_selection_get_selected(o,&model,&iter))
        return naNil();

    path = gtk_tree_model_get_string_from_iter(model,&iter);
    ret = NASTR(path);
    g_free(path);
    return ret;
}

static naRef f_type_children(naContext ctx, naRef me, int argc, naRef* args)
{
    gchar *name = naStr_data(STRARG(0));
    GType t = g_type_from_name(name);
    GType *childs = g_type_children(t,NULL), *p = childs;
    naRef v = naNewVector(ctx);
    for(p = childs; *p; p++)
        naVec_append(v,NASTR(g_type_name(*p)));
    g_free(childs);
    return v;
}

static naRef f_get_type(naContext ctx, naRef me, int argc, naRef* args)
{
    return NASTR(g_type_name(G_TYPE_FROM_INSTANCE(OBJARG(0))));
}

static naRef f_get_signals(naContext ctx, naRef me, int argc, naRef* args)
{
    guint i, nsigs=0;
    GType t = g_type_from_name(naStr_data(STRARG(0)));
    naRef result = naNewVector(ctx);
    GObjectClass* gc = g_type_class_ref(t); /* doesn't work w/o this */
    guint *sigs = g_signal_list_ids(t, &nsigs);
    for(i=0; i<nsigs; i++) {
        GSignalQuery sq;
        g_signal_query(sigs[i], &sq);
        // Technically, should also test the flags against
        // G_SIGNAL_ACTION, but *many* classes forget to set this, so
        // it's basically useless.
        if(sq.itype == t) naVec_append(result, NASTR(sq.signal_name));
    }
    g_free(sigs);
    g_type_class_unref(gc);
    return result;
}

static naRef f_parent_type(naContext ctx, naRef me, int argc, naRef* args)
{
    gchar *type = naStr_data(STRARG(0));
    GType par = g_type_parent(g_type_from_name(type));
    return par?NASTR(g_type_name(par)):naNil();
}

static naRef f_cairo_create(naContext ctx, naRef me, int argc, naRef* args)
{
    cairo_t *cr = gdk_cairo_create(GDK_DRAWABLE(GTK_WIDGET(OBJARG(0))->window));
    return naNewCairoGhost(ctx,cr);
}

static naRef f_set_submenu(naContext ctx, naRef me, int argc, naRef* args)
{
    gtk_menu_item_set_submenu(OBJARG(0), OBJARG(1));
    return naNil();
}

static naRef f_rc_parse_string(naContext ctx, naRef me, int argc, naRef* args)
{
    gtk_rc_parse_string(naStr_data(STRARG(0)));
    return naNil();
}

// Parses LISP-like strings of the form:
//   (gtk_accel_path "<Actions>/Whatever/Something" "<Shift><Control>q")
//   ...
naRef f_accel_map_parse(naContext ctx, naRef me, int argc, naRef* args)
{
    naRef s = STRARG(0);
    GScanner* gs = g_scanner_new(0);
    g_scanner_input_text(gs, naStr_data(s), naStr_len(s));
    gtk_accel_map_load_scanner(gs);
    g_scanner_destroy(gs);
    return naNil();
}

naRef f_window_add_accel_group(naContext ctx, naRef me, int argc, naRef* args)
{
    gtk_window_add_accel_group(OBJARG(0), OBJARG(1));
    return naNil();
}

naRef f_action_set_accel_path(naContext ctx, naRef me, int argc, naRef* args)
{
    gtk_action_set_accel_path(OBJARG(0), naStr_data(STRARG(1)));
    return naNil();
}

naRef f_action_group_add_action(naContext ctx, naRef me, int argc, naRef* args)
{
    gtk_action_group_add_action(OBJARG(0), OBJARG(1));
    return naNil();
}

// Adds all action arguments as a radio group
naRef f_action_group_add_radios(naContext ctx, naRef me, int argc, naRef* args)
{
    int i;
    GSList* list = 0;
    for(i=1; i<argc; i++) {
        gtk_radio_action_set_group(OBJARG(i), list);
        list = gtk_radio_action_get_group(OBJARG(i));
        gtk_action_group_add_action(OBJARG(0), OBJARG(i));
    }
    return naNil();
}

naRef f_ui_manager_insert_action_group(naContext ctx, naRef me,
                                       int argc, naRef* args)
{
    gtk_ui_manager_insert_action_group(OBJARG(0), OBJARG(1), (int)NUMARG(2));
    return naNil();
}

naRef f_ui_manager_get_accel_group(naContext ctx, naRef me,
                                   int argc, naRef* args)
{
    return object2ghost(ctx, (void*)gtk_ui_manager_get_accel_group(OBJARG(0)));
}

naRef f_ui_manager_add_ui(naContext ctx, naRef me, int argc, naRef* args)
{
    GError* err = 0;
    naRef s = STRARG(1);
    int id = gtk_ui_manager_add_ui_from_string(OBJARG(0), naStr_data(s),
                                               naStr_len(s), &err);
    if(!id) naRuntimeError(ctx, err->message);
    return naNum(id);
}

naRef f_ui_manager_remove_ui(naContext ctx, naRef me, int argc, naRef* args)
{
    gtk_ui_manager_remove_ui(OBJARG(0), (guint)NUMARG(1));
    return naNil();
}

naRef f_ui_manager_get_widget(naContext ctx, naRef me, int argc, naRef* args)
{
    const char* s = naStr_data(STRARG(1));
    return object2ghost(ctx, (void*)gtk_ui_manager_get_widget(OBJARG(0), s));
}

naRef f_toggle_action_get_active(naContext ctx, naRef me, int argc, naRef* args)
{
    return naNum(gtk_toggle_action_get_active(OBJARG(0)));
}

naRef f_toggle_action_set_active(naContext ctx, naRef me, int argc, naRef* args)
{
    gtk_toggle_action_set_active(OBJARG(0), NUMARG(1) != 0);
    return naNil();
}

static naCFuncItem funcs[] = {
// Needed to make UIManager & menus work (yuck!):
    { "accel_map_parse", f_accel_map_parse },
    { "window_add_accel_group", f_window_add_accel_group },
    { "action_set_accel_path", f_action_set_accel_path },
    { "action_group_add_action", f_action_group_add_action },
    { "action_group_add_radios", f_action_group_add_radios },
    { "ui_manager_insert_action_group", f_ui_manager_insert_action_group },
    { "ui_manager_get_accel_group", f_ui_manager_get_accel_group },
    { "ui_manager_add_ui", f_ui_manager_add_ui },
    { "ui_manager_remove_ui", f_ui_manager_remove_ui },
    { "ui_manager_get_widget", f_ui_manager_get_widget },
    { "menu_item_set_submenu", f_set_submenu },
    { "toggle_action_get_active", f_toggle_action_get_active },
    { "toggle_action_set_active", f_toggle_action_set_active },
//special methods
    { "list_store_new", f_list_store_new },
    { "list_store_append", f_list_store_append },
    { "list_store_remove", f_list_store_remove },
    { "list_store_set", f_list_store_set },
    { "list_store_clear", f_list_store_clear },
    { "tree_view_get_selection", f_tree_view_get_selection },
    { "tree_view_column_add_cell", f_column_add_cell },
    { "tree_view_append_column", f_append_column },
    { "tree_selection_get_selected", f_tree_selection_get_selected },
    { "tree_model_get", f_tree_model_get },
    { "text_view_insert", f_text_insert},
    { "text_view_scroll_to_cursor", f_text_scroll_to_cursor},
    { "widget_cairo_create", f_cairo_create },
    { "widget_queue_draw", f_queue_draw },
    { "widget_show_all", f_show_all },
    { "rc_parse_string", f_rc_parse_string },
//core stuff
    { "get_signals", f_get_signals },
    { "parent_type", f_parent_type },
    { "get_type", f_get_type },
    { "type_children", f_type_children },
    { "new", f_new },
    { "init", f_init },
    { "main", f_main },
    { "main_quit", f_main_quit },
    { "set", f_set },
    { "get", f_get },
    { "child_set", f_child_set },
    { "child_get", f_child_get },
    { "connect", f_connect },
    { "emit", f_emit },
    { "timeout_add", f_timeout_add },
    { "source_remove", f_source_remove },
    { 0 }
};

naRef naInit_gtk(naContext ctx) {
    namespace = naGenLib(ctx,funcs);
    closures = naNewHash(ctx);
    naSave(ctx,closures);
    timers = naNewHash(ctx);
    naSave(ctx,timers);
    return namespace;
}
