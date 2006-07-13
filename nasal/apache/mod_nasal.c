#include <string.h>
#include <stdio.h>
#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "util_filter.h"
#include "util_script.h"
#include "http_log.h"

#include "nasal.h"

/* To build (assuming there's a libnasal.a in the local directory):
 *   apxs -i -a -c -Wc,-Wall -Wc,-Werror -L. -lnasal mod_nasal.c
 *
 * The -Werror is to force a failure before it tries to install a
 * module with warnings.  Then just restart apache.
 */

#define LOGERR(svr, ...) \
    ap_log_error(__FILE__, __LINE__, APLOG_ERR, 0, (svr), __VA_ARGS__)

/* Wrapper for a single handler request, mostly used for the bucket
 * brigade reading scheme.  Zero fill and initialize the r field, then
 * just call read_next_buf() and use buf/len fields (returns zero to
 * indicate EOS or error) */
struct nasal_request {
    request_rec* r;
    apr_bucket_brigade* bb;
    apr_bucket* b;
    const char* buf;
    apr_size_t len;
};

/* TODO: APR synchronization functions:
 * mutex_new, mutex_lock, mutex_unlock, cond_new, cond_wait,
 * cond_signal, cond_broadcast
 */

/* Nasal environments are server-specific.  The handlers is a hash
 * table to look up handler functions from their names (defined with
 * the NasalHandler configuration directive).  The namespace is a hash
 * of symbols that will be copied into new file contexts after the
 * standard library symbols (note: copied this is not an outer lexical
 * context that is shared between handlers -- it is a template used
 * for creating new contexts). */
struct nasal_cfg {
    naRef handlers;  /* name -> handler function */
    naRef namespace; /* hash table */
};

#define NASTR(s) naStr_fromdata(naNewString(ctx), (void*)(s), strlen((s)))

static void copy_hash(naContext ctx, naRef src, naRef dst)
{
    int i;
    naRef keys = naNewVector(ctx);
    naHash_keys(keys, src);
    for(i=0; i<naVec_size(keys); i++) {
        naRef val, key = naVec_get(keys, i);
        naHash_get(src, key, &val);
        naHash_set(dst, key, val);
    }
}

static void* read_file(const char* file, apr_pool_t* pool, int* len)
{
    apr_size_t n;
    void* buf;
    apr_finfo_t finf;
    apr_file_t *fd = NULL;

    if(apr_stat(&finf, file, APR_FINFO_SIZE, pool)) goto ERROUT;
    if(apr_file_open(&fd, file, APR_READ, 0, pool)) goto ERROUT;
    if(!(buf = apr_palloc(pool, finf.size))) goto ERROUT;
    if(apr_file_read_full(fd, buf, finf.size, &n)) goto ERROUT;
    if(n != finf.size) goto ERROUT;

    *len = finf.size;
    return buf;

 ERROUT:
    if(fd) apr_file_close(fd);
    return NULL;
}

static void dumpStack(naContext ctx, const server_rec* s)
{
    int i;
    LOGERR(s,"%s at %s, line %d", naGetError(ctx),
           naStr_data(naGetSourceFile(ctx, 0)), naGetLine(ctx, 0));
    for(i=1; i<naStackDepth(ctx); i++)
        LOGERR(s, "  called from: %s, line %d",
               naStr_data(naGetSourceFile(ctx, i)), naGetLine(ctx, i));
}

/* Uses Apache's funky bucket brigade filter API to read a chunk (how
 * big depends on the upstream filters) of data into the nasal_request
 * object's buf/len fields.  Returns 0 on error or end-of-stream, 1 if
 * more data is expected. */
static int read_next_buf(struct nasal_request* nr)
{
    int st;
    if(!nr->bb)
        nr->bb = apr_brigade_create(nr->r->pool,
                                    nr->r->connection->bucket_alloc);
    if(!nr->bb) return 0;
    while(1) {
        if(nr->b) {
            nr->b = APR_BUCKET_NEXT(nr->b);
        } else {
            st = ap_get_brigade(nr->r->input_filters, nr->bb, AP_MODE_READBYTES,
                                APR_BLOCK_READ, HUGE_STRING_LEN);
            if(st != APR_SUCCESS) return 0;
            nr->b = APR_BRIGADE_FIRST(nr->bb);
        }
        if(nr->b == APR_BRIGADE_SENTINEL(nr->bb)) {
            apr_brigade_cleanup(nr->bb);
            nr->b = NULL;
            continue;
        }
        if(APR_BUCKET_IS_FLUSH(nr->b)) continue;
        if(APR_BUCKET_IS_EOS(nr->b)) return 0;
        apr_bucket_read(nr->b, &nr->buf, &nr->len, APR_BLOCK_READ);
        return 1;
    }
}

static naRef f_print(naContext c, naRef me, int argc, naRef* args)
{
    int i;
    struct nasal_request* nr = naGetUserData(c);
    if(!nr) naRuntimeError(c, "print() called outside of request");
    for(i=0; i<argc; i++) {
        naRef s = naStringValue(c, args[i]);
        if(naIsNil(s)) continue;
        ap_rwrite(naStr_data(s), naStr_len(s), nr->r);
    }
    return naNil();
}

static naRef f_read(naContext c, naRef me, int argc, naRef* args)
{
    struct nasal_request* nr = naGetUserData(c);
    if(nr && read_next_buf(nr))
        return naStr_fromdata(naNewString(c), (void*)nr->buf, nr->len);
    return naNil();
}

static naRef f_getcgi(naContext ctx, naRef me, int argc, naRef* args)
{
    const char* val;
    struct nasal_request* nr = naGetUserData(ctx);
    naRef var = args > 0 ? naStringValue(ctx, args[0]) : naNil();
    if(!nr) naRuntimeError(ctx, "getcgi() called outside of request");
    if(naIsNil(var)) naRuntimeError(ctx, "Bad argument to getcgi()");
    val = apr_table_get(nr->r->subprocess_env, naStr_data(var));
    return val ? NASTR(val) : naNil();
}

static naRef f_gethdr(naContext ctx, naRef me, int argc, naRef* args)
{
    const char* val;
    struct nasal_request* nr = naGetUserData(ctx);
    naRef var = args > 0 ? naStringValue(ctx, args[0]) : naNil();
    if(!nr) naRuntimeError(ctx, "gethdr() called outside of request");
    if(naIsNil(var)) naRuntimeError(ctx, "Bad argument to gethdr()");
    val = apr_table_get(nr->r->headers_in, naStr_data(var));
    return val ? NASTR(val) : naNil();
}

static naRef f_sethdr(naContext ctx, naRef me, int argc, naRef* args)
{
    naRef var = argc > 0 ? naStringValue(ctx, args[0]) : naNil();
    naRef val = argc > 1 ? naStringValue(ctx, args[1]) : naNil();
    struct nasal_request* nr = naGetUserData(ctx);
    if(!nr || naIsNil(var) || naIsNil(val))
        naRuntimeError(ctx, "Bad argument/context for sethdr()");
    apr_table_set(nr->r->headers_out, naStr_data(var), naStr_data(val));
    return val;
}

static naRef f_setstatus(naContext ctx, naRef me, int argc, naRef* args)
{
    naRef val = args > 0 ? naNumValue(args[0]) : naNil();
    struct nasal_request* nr = naGetUserData(ctx);
    if(!nr || naIsNil(val))
        naRuntimeError(ctx, "Bad argument/context for setstatus()");
    nr->r->status = (int)val.num;
    return val;
}

struct func { char* name; naCFunction func; };
static struct func funcs[] = {
    { "print", f_print },
    { "read", f_read },
    { "getcgi", f_getcgi },
    { "gethdr", f_gethdr },
    { "sethdr", f_sethdr },
    { "setstatus", f_setstatus },
};
static naRef make_syms(naContext ctx)
{
    naRef syms = naStdLib(ctx);
    int i, n = sizeof(funcs)/sizeof(struct func);
    for(i=0; i<n; i++) {
        naRef code = naNewCCode(ctx, funcs[i].func);
        naRef name = NASTR(funcs[i].name);
        name = naInternSymbol(name);
        naHash_set(syms, name, naNewFunc(ctx, code));
    }
    return syms;
}

static naRef run_file(naContext ctx, struct nasal_cfg* cfg, const char* file,
                      cmd_parms* cmd, const char** err)
{
    int len, errline;
    void* buf;
    naRef code, syms, result;
    
    if(!(buf = read_file(file, cmd->temp_pool, &len))) {
        *err = "error reading Nasal file";
        return naNil();
    }
    if(naIsNil(code = naParseCode(ctx, NASTR(file), 1, buf, len, &errline)))
    {
        LOGERR(cmd->server, "Parse error in %s: %s at line %d",
                     file, naGetError(ctx), errline);
        *err = "parse error in Nasal file";
        return naNil();
    }
    syms = make_syms(ctx);
    naHash_set(syms, naInternSymbol(NASTR("math")),  naMathLib(ctx));
    naHash_set(syms, naInternSymbol(NASTR("bits")),  naBitsLib(ctx));
    naHash_set(syms, naInternSymbol(NASTR("io")),    naIOLib(ctx));
    naHash_set(syms, naInternSymbol(NASTR("regex")), naRegexLib(ctx));
    naHash_set(syms, naInternSymbol(NASTR("unix")),  naUnixLib(ctx));
    naHash_set(syms, naInternSymbol(NASTR("utf8")),  naUtf8Lib(ctx));
    copy_hash(ctx, cfg->namespace, syms); 

    result = naCall(ctx, code, 0, 0, naNil(), syms);
    if(naIsNil(result) && naGetError(ctx)) {
        dumpStack(ctx, cmd->server);
        *err = "runtime error in Nasal file";
        return naNil();
    }
    *err = NULL;
    return result;
}

/*
 * Apache stuff below:
 */

module AP_MODULE_DECLARE_DATA nasal_module;

static int nasal_handle_request(request_rec *r)
{
    struct nasal_request nreq;
    naContext ctx;
    naRef handler;
    struct nasal_cfg *cfg = ap_get_module_config(r->server->module_config,
                                                 &nasal_module);
    /* Is it ours? */
    /* FIXME: make naHash_cget() work here so we can skip
     * naNewContext() for stuff we don't handle. */
    ctx = naNewContext();
    if(!naHash_get(cfg->handlers, NASTR(r->handler), &handler)) {
        naFreeContext(ctx);
        return DECLINED;
    }

    /* Initialize CGI "environment" vars and our bucket reader */
    ap_add_common_vars(r);
    ap_add_cgi_vars(r);
    memset(&nreq, 0, sizeof(nreq));
    nreq.r = r;
    r->status = HTTP_OK;

    /* Call the handler */
    ctx = naNewContext();
    naSetUserData(ctx, &nreq);
    naCall(ctx, handler, 0, 0, naNil(), naNil());
    if(naGetError(ctx)) {
        dumpStack(ctx, r->server);
        r->status = HTTP_INTERNAL_SERVER_ERROR;
    }
    naFreeContext(ctx);

    /* FIXME: what's the proper behavior here?  I have to return "OK"
     * if I want the generated page to show.  But I have to return the
     * status (and *not* "OK") if I want Apache's internal error
     * handling to get hooked. */
    return r->status < 400 ? OK : r->status;
}

static void register_hooks(apr_pool_t *p)
{
    ap_hook_handler(nasal_handle_request, NULL, NULL, APR_HOOK_MIDDLE);
}

/* Run the file in the current environment.  The result is a code
 * object to be used as the handler. */
static const char *cmd_nasal_handler(cmd_parms *cmd, void *whatisthis, 
                                     const char *file, const char *handler)
{
    const char* err = NULL;
    struct nasal_cfg *cfg = ap_get_module_config(cmd->server->module_config,
                                                 &nasal_module);
    naContext ctx = naNewContext();
    naRef code = run_file(ctx, cfg, file, cmd, &err);
    if(!err) {
        if(!naIsFunc(code))
            err = "NasalHandler code did not return a function";
        else
            naHash_set(cfg->handlers, NASTR(handler), code);
    }
    naFreeContext(ctx);
    return err;
}

/* Run the file in the current environment.  The result is a hash
 * table of symbols to add to the environment. */
static const char *cmd_nasal_init(cmd_parms *cmd, void *whatisthis, 
                                  const char *file)
{
    const char* err = NULL;
    struct nasal_cfg *cfg = ap_get_module_config(cmd->server->module_config,
                                                 &nasal_module);
    naContext ctx = naNewContext();
    naRef syms = run_file(ctx, cfg, file, cmd, &err);
    if(!err) {
        if(naIsHash(syms)) copy_hash(ctx, syms, cfg->namespace);
        else               err = "NasalInit code did not return a hash table";
    }
    naFreeContext(ctx);
    return err;
}

static const command_rec nasal_cmds[] = {
    AP_INIT_TAKE1("NasalInit", cmd_nasal_init, NULL, RSRC_CONF,
                  "A Nasal file to initialize the server-global namespace"),
    AP_INIT_TAKE2("NasalHandler", cmd_nasal_handler, NULL, RSRC_CONF,
                  "A Nasal file to load and name for the resulting handler"),
    {NULL}
};

static void *nasal_create_server_config(apr_pool_t *p, server_rec *s)
{
    naContext ctx = naNewContext();
    struct nasal_cfg *cfg = apr_pcalloc(p, sizeof(struct nasal_cfg));
    naSave(ctx, cfg->namespace = naNewHash(ctx));
    naSave(ctx, cfg->handlers = naNewHash(ctx));
    naFreeContext(ctx);
    return cfg;
}

module AP_MODULE_DECLARE_DATA nasal_module =
{
    STANDARD20_MODULE_STUFF,
    NULL,           /* create_dir_config */
    NULL,           /* merge_dir_config */
    nasal_create_server_config, /* create_server_config */
    NULL,           /* merge_server_config */
    nasal_cmds,     /* cmds */
    register_hooks

};
