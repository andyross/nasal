#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <cairo/cairo.h>

#include "nasal.h"

typedef struct { cairo_t *cr; } cairoGhost;

static void cairoGhostDestroy(cairoGhost *g) { free(g); }

static naGhostType cairoGhostType = { (void(*)(void*))cairoGhostDestroy };

naRef new_cairoGhost(naContext ctx, cairo_t *cr) {
    cairoGhost *g = malloc(sizeof(cairoGhost));
    g->cr = cr;
    return naNewGhost(ctx,&cairoGhostType,g);
}

static cairo_t *ghost2cairo(naRef r)
{
    if(naGhost_type(r) != &cairoGhostType)
        return NULL;
    return ((cairoGhost*)naGhost_ptr(r))->cr;
}

static void check_argc(naContext c, int argc, int n, char *f) {
    if(n>=argc) naRuntimeError(c,"Too few args to %s()",f);
}

static cairo_t *arg_cairo(naContext c, int argc, naRef *a, int n, char *f)
{
    cairo_t *cr;
    check_argc(c,argc,n,f);
    cr = ghost2cairo(a[n]);
    if(!cr) naRuntimeError(c,"Arg %d to %s() not a cairo context",n+1,f);
    return cr;
}

static naRef arg_str(naContext c, int argc, naRef *a,int n, char *f) {
    naRef r;
    check_argc(c,argc,n,f);
    r = naStringValue(c,a[n]);
    if(!naIsString(r))
        naRuntimeError(c,"Arg %d to %s() not a string",n+1,f);
    return r;
}

static double arg_num(naContext c, int argc, naRef *a,int n, char *f) {
    naRef r;
    check_argc(c,argc,n,f);
    r = naNumValue(a[n]);
    if(!naIsNum(r))
        naRuntimeError(c,"Arg %d to %s() not a num",n+1,f);
    return r.num;
}

#define CAIRO_0ARG(x) \
static naRef f_##x(naContext ctx, naRef me, int argc, naRef* args) { \
    char *fn = "cairo."#x; cairo_t *cr = arg_cairo(ctx,argc,args,0,fn); \
    cairo_##x(cr); return naNil(); }

#define CAIRO_1ARG(x) \
static naRef f_##x(naContext ctx, naRef me, int argc, naRef* args) { \
    char *fn = "cairo."#x; cairo_t *cr = arg_cairo(ctx,argc,args,0,fn); \
    double a = arg_num(ctx,argc,args,1,fn); \
    cairo_##x(cr,a); return naNil(); }

#define CAIRO_2ARG(x) \
static naRef f_##x(naContext ctx, naRef me, int argc, naRef* args) { \
    char *fn = "cairo."#x; cairo_t *cr = arg_cairo(ctx,argc,args,0,fn); \
    double a = arg_num(ctx,argc,args,1,fn); double b = arg_num(ctx,argc,args,2,fn); \
    cairo_##x(cr,a,b); return naNil(); }

#define CAIRO_3ARG(x) \
static naRef f_##x(naContext ctx, naRef me, int argc, naRef* args) { \
    char *fn = "cairo."#x; cairo_t *cr = arg_cairo(ctx,argc,args,0,fn); \
    double a = arg_num(ctx,argc,args,1,fn); double b = arg_num(ctx,argc,args,2,fn); \
    double c = arg_num(ctx,argc,args,3,fn); \
    cairo_##x(cr,a,b,c); return naNil(); }

#define CAIRO_4ARG(x) \
static naRef f_##x(naContext ctx, naRef me, int argc, naRef* args) { \
    char *fn = "cairo."#x; cairo_t *cr = arg_cairo(ctx,argc,args,0,fn); \
    double a = arg_num(ctx,argc,args,1,fn); double b = arg_num(ctx,argc,args,2,fn); \
    double c = arg_num(ctx,argc,args,3,fn); double d = arg_num(ctx,argc,args,4,fn); \
    cairo_##x(cr,a,b,c,d); return naNil(); }

CAIRO_0ARG(stroke)
CAIRO_0ARG(stroke_preserve)
CAIRO_0ARG(fill)
CAIRO_0ARG(fill_preserve)
CAIRO_0ARG(show_page)
CAIRO_0ARG(copy_page)
CAIRO_0ARG(save)
CAIRO_0ARG(restore)
CAIRO_0ARG(new_path)
CAIRO_0ARG(close_path)

CAIRO_1ARG(set_line_width)
CAIRO_1ARG(set_font_size)

CAIRO_2ARG(move_to)
CAIRO_2ARG(line_to)
CAIRO_2ARG(rel_move_to)
CAIRO_2ARG(rel_line_to)
CAIRO_2ARG(scale)

CAIRO_3ARG(set_source_rgb)

CAIRO_4ARG(set_source_rgba)
CAIRO_4ARG(rectangle)

#undef CAIRO_0ARG
#undef CAIRO_1ARG
#undef CAIRO_2ARG
#undef CAIRO_3ARG
#undef CAIRO_4ARG

static naRef f_show_text(naContext ctx, naRef me, int argc, naRef* args)
{
    char *fn = "cairo_show_text";
    cairo_t *cr = arg_cairo(ctx,argc,args,0,fn);
    const char *s = naStr_data(arg_str(ctx,argc,args,1,fn));
    cairo_show_text(cr,s);
    return naNil();
}

static naRef f_set_dash(naContext ctx, naRef me, int argc, naRef* args)
{
    char *fn = "cairo_set_dash";
    cairo_t *cr = arg_cairo(ctx,argc,args,0,fn);
    naRef v = (argc>1 && naIsVector(args[1]))?args[1]:naNil();
    int i,sz = naVec_size(v);
    double *dashes = malloc(sizeof(double)*sz);
    double offset = argc>2?arg_num(ctx,argc,args,2,fn):0;
    for(i=0;i<sz;i++)
        dashes[i] = naNumValue(naVec_get(v,i)).num;
    cairo_set_dash(cr,dashes,sz,offset);
    return naNil();
}

static naCFuncItem funcs[] = {
    { "set_dash", f_set_dash },
    { "rectangle", f_rectangle },
    { "close_path", f_close_path },
    { "new_path", f_new_path },
    { "save", f_save },
    { "restore", f_restore },
    { "scale", f_scale },
    { "set_font_size", f_set_font_size },
    { "set_line_width", f_set_line_width },
    { "set_source_rgba", f_set_source_rgba },
    { "set_source_rgb", f_set_source_rgb },
    { "show_page", f_show_page },
    { "copy_page", f_copy_page },
    { "stroke", f_stroke },
    { "stroke_preserve", f_stroke_preserve },
    { "fill", f_fill },
    { "fill_preserve", f_fill_preserve },
    { "move_to", f_move_to },
    { "line_to", f_line_to },
    { "rel_move_to", f_rel_move_to },
    { "rel_line_to", f_rel_line_to },
    { "show_text", f_show_text },
    { 0 }
};

naRef naInit_cairo(naContext ctx) {
    return naGenLib(ctx,funcs);
}

