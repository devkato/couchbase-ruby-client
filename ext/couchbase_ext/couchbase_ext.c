/* vim: ft=c et ts=8 sts=4 sw=4 cino=
 *
 *   Copyright 2011, 2012 Couchbase, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ruby.h>
#ifndef RUBY_ST_H
#include <st.h>
#endif

#include <time.h>
#include <libcouchbase/couchbase.h>
#include "couchbase_config.h"

#ifdef HAVE_STDARG_PROTOTYPES
#include <stdarg.h>
#define va_init_list(a,b) va_start(a,b)
#else
#include <varargs.h>
#define va_init_list(a,b) va_start(a)
#endif

#define debug_object(OBJ) \
    rb_funcall(rb_stderr, rb_intern("print"), 1, rb_funcall(OBJ, rb_intern("object_id"), 0)); \
    rb_funcall(rb_stderr, rb_intern("print"), 1, rb_str_new2(" ")); \
    rb_funcall(rb_stderr, rb_intern("print"), 1, rb_funcall(OBJ, rb_intern("class"), 0)); \
    rb_funcall(rb_stderr, rb_intern("print"), 1, rb_str_new2(" ")); \
    rb_funcall(rb_stderr, rb_intern("puts"), 1, rb_funcall(OBJ, rb_intern("inspect"), 0));

#define FMT_MASK        0x3
#define FMT_DOCUMENT    0x0
#define FMT_MARSHAL     0x1
#define FMT_PLAIN       0x2

typedef struct
{
    libcouchbase_t handle;
    struct libcouchbase_io_opt_st *io;
    uint16_t port;
    char *authority;
    char *hostname;
    char *pool;
    char *bucket;
    char *username;
    char *password;
    int async;
    int quiet;
    long seqno;
    VALUE default_format;    /* should update +default_flags+ on change */
    uint32_t default_flags;
    time_t default_ttl;
    uint32_t timeout;
    VALUE exception;        /* error delivered by error_callback */
    VALUE on_error_proc;    /* is using to deliver errors in async mode */
    VALUE object_space;
    char *node_list;
} bucket_t;

typedef struct
{
    bucket_t* bucket;
    int extended;
    VALUE proc;
    void *rv;
    VALUE exception;
    VALUE force_format;
    int quiet;
    int arithm;           /* incr: +1, decr: -1, other: 0 */
} context_t;

struct key_traits
{
    VALUE keys_ary;
    size_t nkeys;
    char **keys;
    libcouchbase_size_t *lens;
    time_t *ttls;
    int extended;
    int explicit_ttl;
    int quiet;
    int mgat;
    VALUE force_format;
};

static VALUE mCouchbase, mError, mJSON, mURI, mMarshal, cBucket, cResult;

static ID  sym_add,
           sym_append,
           sym_bucket,
           sym_cas,
           sym_create,
           sym_decrement,
           sym_default_flags,
           sym_default_format,
           sym_default_ttl,
           sym_delete,
           sym_document,
           sym_extended,
           sym_flags,
           sym_flush,
           sym_format,
           sym_get,
           sym_hostname,
           sym_increment,
           sym_initial,
           sym_marshal,
           sym_password,
           sym_plain,
           sym_pool,
           sym_port,
           sym_prepend,
           sym_quiet,
           sym_replace,
           sym_set,
           sym_stats,
           sym_timeout,
           sym_touch,
           sym_ttl,
           sym_username,
           sym_version,
           sym_node_list,
           id_arity,
           id_call,
           id_dump,
           id_flatten_bang,
           id_has_key_p,
           id_host,
           id_iv_cas,
           id_iv_error,
           id_iv_flags,
           id_iv_key,
           id_iv_node,
           id_iv_operation,
           id_iv_value,
           id_dup,
           id_load,
           id_match,
           id_parse,
           id_password,
           id_path,
           id_port,
           id_scheme,
           id_to_s,
           id_user;

/* base error */
static VALUE eBaseError;
static VALUE eValueFormatError;

/* libcouchbase errors */
                                       /*LIBCOUCHBASE_SUCCESS = 0x00*/
                                       /*LIBCOUCHBASE_AUTH_CONTINUE = 0x01*/
static VALUE eAuthError;               /*LIBCOUCHBASE_AUTH_ERROR = 0x02*/
static VALUE eDeltaBadvalError;        /*LIBCOUCHBASE_DELTA_BADVAL = 0x03*/
static VALUE eTooBigError;             /*LIBCOUCHBASE_E2BIG = 0x04*/
static VALUE eBusyError;               /*LIBCOUCHBASE_EBUSY = 0x05*/
static VALUE eInternalError;           /*LIBCOUCHBASE_EINTERNAL = 0x06*/
static VALUE eInvalidError;            /*LIBCOUCHBASE_EINVAL = 0x07*/
static VALUE eNoMemoryError;           /*LIBCOUCHBASE_ENOMEM = 0x08*/
static VALUE eRangeError;              /*LIBCOUCHBASE_ERANGE = 0x09*/
static VALUE eLibcouchbaseError;       /*LIBCOUCHBASE_ERROR = 0x0a*/
static VALUE eTmpFailError;            /*LIBCOUCHBASE_ETMPFAIL = 0x0b*/
static VALUE eKeyExistsError;          /*LIBCOUCHBASE_KEY_EEXISTS = 0x0c*/
static VALUE eNotFoundError;           /*LIBCOUCHBASE_KEY_ENOENT = 0x0d*/
static VALUE eLibeventError;           /*LIBCOUCHBASE_LIBEVENT_ERROR = 0x0e*/
static VALUE eNetworkError;            /*LIBCOUCHBASE_NETWORK_ERROR = 0x0f*/
static VALUE eNotMyVbucketError;       /*LIBCOUCHBASE_NOT_MY_VBUCKET = 0x10*/
static VALUE eNotStoredError;          /*LIBCOUCHBASE_NOT_STORED = 0x11*/
static VALUE eNotSupportedError;       /*LIBCOUCHBASE_NOT_SUPPORTED = 0x12*/
static VALUE eUnknownCommandError;     /*LIBCOUCHBASE_UNKNOWN_COMMAND = 0x13*/
static VALUE eUnknownHostError;        /*LIBCOUCHBASE_UNKNOWN_HOST = 0x14*/
static VALUE eProtocolError;           /*LIBCOUCHBASE_PROTOCOL_ERROR = 0x15*/
static VALUE eTimeoutError;            /*LIBCOUCHBASE_ETIMEDOUT = 0x16*/
static VALUE eConnectError;            /*LIBCOUCHBASE_CONNECT_ERROR = 0x17*/
static VALUE eBucketNotFoundError;     /*LIBCOUCHBASE_BUCKET_ENOENT = 0x18*/

    static void
cb_gc_protect(bucket_t *bucket, VALUE val)
{
    rb_hash_aset(bucket->object_space, val|1, val);
}

    static void
cb_gc_unprotect(bucket_t *bucket, VALUE val)
{
    rb_hash_delete(bucket->object_space, val|1);
}

    static VALUE
cb_proc_call(VALUE recv, int argc, ...)
{
    VALUE *argv;
    va_list ar;
    int arity;
    int ii;

    arity = FIX2INT(rb_funcall(recv, id_arity, 0));
    if (arity < 0) {
        arity = argc;
    }
    if (arity > 0) {
        va_init_list(ar, argc);
        argv = ALLOCA_N(VALUE, argc);
        for (ii = 0; ii < arity; ++ii) {
            if (ii < argc) {
                argv[ii] = va_arg(ar, VALUE);
            } else {
                argv[ii] = Qnil;
            }
        }
        va_end(ar);
    } else {
        argv = NULL;
    }
    return rb_funcall2(recv, id_call, arity, argv);
}

/* Helper to conver return code from libcouchbase to meaningful exception.
 * Returns nil if the code considering successful and exception object
 * otherwise. Store given string to exceptions as message, and also
 * initialize +error+ attribute with given return code.  */
    static VALUE
cb_check_error(libcouchbase_error_t rc, const char *msg, VALUE key)
{
    VALUE klass, exc, str;
    char buf[300];

    if (rc == LIBCOUCHBASE_SUCCESS || rc == LIBCOUCHBASE_AUTH_CONTINUE) {
        return Qnil;
    }
    switch (rc) {
        case LIBCOUCHBASE_AUTH_ERROR:
            klass = eAuthError;
            break;
        case LIBCOUCHBASE_DELTA_BADVAL:
            klass = eDeltaBadvalError;
            break;
        case LIBCOUCHBASE_E2BIG:
            klass = eTooBigError;
            break;
        case LIBCOUCHBASE_EBUSY:
            klass = eBusyError;
            break;
        case LIBCOUCHBASE_EINTERNAL:
            klass = eInternalError;
            break;
        case LIBCOUCHBASE_EINVAL:
            klass = eInvalidError;
            break;
        case LIBCOUCHBASE_ENOMEM:
            klass = eNoMemoryError;
            break;
        case LIBCOUCHBASE_ERANGE:
            klass = eRangeError;
            break;
        case LIBCOUCHBASE_ETMPFAIL:
            klass = eTmpFailError;
            break;
        case LIBCOUCHBASE_KEY_EEXISTS:
            klass = eKeyExistsError;
            break;
        case LIBCOUCHBASE_KEY_ENOENT:
            klass = eNotFoundError;
            break;
        case LIBCOUCHBASE_LIBEVENT_ERROR:
            klass = eLibeventError;
            break;
        case LIBCOUCHBASE_NETWORK_ERROR:
            klass = eNetworkError;
            break;
        case LIBCOUCHBASE_NOT_MY_VBUCKET:
            klass = eNotMyVbucketError;
            break;
        case LIBCOUCHBASE_NOT_STORED:
            klass = eNotStoredError;
            break;
        case LIBCOUCHBASE_NOT_SUPPORTED:
            klass = eNotSupportedError;
            break;
        case LIBCOUCHBASE_UNKNOWN_COMMAND:
            klass = eUnknownCommandError;
            break;
        case LIBCOUCHBASE_UNKNOWN_HOST:
            klass = eUnknownHostError;
            break;
        case LIBCOUCHBASE_PROTOCOL_ERROR:
            klass = eProtocolError;
            break;
        case LIBCOUCHBASE_ETIMEDOUT:
            klass = eTimeoutError;
            break;
        case LIBCOUCHBASE_CONNECT_ERROR:
            klass = eConnectError;
            break;
        case LIBCOUCHBASE_BUCKET_ENOENT:
            klass = eBucketNotFoundError;
            break;
        case LIBCOUCHBASE_ERROR:
            /* fall through */
        default:
            klass = eLibcouchbaseError;
    }

    str = rb_str_buf_new2(msg ? msg : "");
    rb_str_buf_cat2(str, " (");
    if (key != Qnil) {
        snprintf(buf, 300, "key=\"%s\", ", RSTRING_PTR(key));
        rb_str_buf_cat2(str, buf);
    }
    snprintf(buf, 300, "error=0x%02x)", rc);
    rb_str_buf_cat2(str, buf);
    exc = rb_exc_new3(klass, str);
    rb_ivar_set(exc, id_iv_error, INT2FIX(rc));
    rb_ivar_set(exc, id_iv_key, key);
    rb_ivar_set(exc, id_iv_cas, Qnil);
    rb_ivar_set(exc, id_iv_operation, Qnil);
    return exc;
}

    static inline uint32_t
flags_set_format(uint32_t flags, ID format)
{
    flags &= ~((uint32_t)FMT_MASK); /* clear format bits */

    if (format == sym_document) {
        return flags | FMT_DOCUMENT;
    } else if (format == sym_marshal) {
        return flags | FMT_MARSHAL;
    } else if (format == sym_plain) {
        return flags | FMT_PLAIN;
    }
    return flags; /* document is the default */
}

    static inline ID
flags_get_format(uint32_t flags)
{
    flags &= FMT_MASK; /* select format bits */

    switch (flags) {
        case FMT_DOCUMENT:
            return sym_document;
        case FMT_MARSHAL:
            return sym_marshal;
        case FMT_PLAIN:
            /* fall through */
        default:
            /* all other formats treated as plain */
            return sym_plain;
    }
}


    static VALUE
do_encode(VALUE *args)
{
    VALUE val = args[0];
    uint32_t flags = ((uint32_t)args[1] & FMT_MASK);

    switch (flags) {
        case FMT_DOCUMENT:
            return rb_funcall(mJSON, id_dump, 1, val);
        case FMT_MARSHAL:
            return rb_funcall(mMarshal, id_dump, 1, val);
        case FMT_PLAIN:
            /* fall through */
        default:
            /* all other formats treated as plain */
            return val;
    }
}

    static VALUE
do_decode(VALUE *args)
{
    VALUE blob = args[0];
    VALUE force_format = args[2];

    if (TYPE(force_format) == T_SYMBOL) {
        if (force_format == sym_document) {
            return rb_funcall(mJSON, id_load, 1, blob);
        } else if (force_format == sym_marshal) {
            return rb_funcall(mMarshal, id_load, 1, blob);
        } else { /* sym_plain and any other symbol */
            return blob;
        }
    } else {
        uint32_t flags = ((uint32_t)args[1] & FMT_MASK);

        switch (flags) {
            case FMT_DOCUMENT:
                return rb_funcall(mJSON, id_load, 1, blob);
            case FMT_MARSHAL:
                return rb_funcall(mMarshal, id_load, 1, blob);
            case FMT_PLAIN:
                /* fall through */
            default:
                /* all other formats treated as plain */
                return blob;
        }
    }
}

    static VALUE
coding_failed(void)
{
    return Qundef;
}

    static VALUE
encode_value(VALUE val, uint32_t flags)
{
    VALUE blob, args[2];

    args[0] = val;
    args[1] = (VALUE)flags;
    blob = rb_rescue(do_encode, (VALUE)args, coding_failed, 0);
    /* it must be bytestring after all */
    if (TYPE(blob) != T_STRING) {
        return Qundef;
    }
    return blob;
}

    static VALUE
decode_value(VALUE blob, uint32_t flags, VALUE force_format)
{
    VALUE val, args[3];

    /* first it must be bytestring */
    if (TYPE(blob) != T_STRING) {
        return Qundef;
    }
    args[0] = blob;
    args[1] = (VALUE)flags;
    args[2] = (VALUE)force_format;
    val = rb_rescue(do_decode, (VALUE)args, coding_failed, 0);
    return val;
}

    static VALUE
unify_key(VALUE key)
{
    switch (TYPE(key)) {
        case T_STRING:
            return key;
        case T_SYMBOL:
            return rb_str_new2(rb_id2name(SYM2ID(key)));
        default:    /* call #to_str or raise error */
            return StringValue(key);
    }
}

    static int
cb_extract_keys_i(VALUE key, VALUE value, VALUE arg)
{
    struct key_traits *traits = (struct key_traits *)arg;
    key = unify_key(key);
    rb_ary_push(traits->keys_ary, key);
    traits->keys[traits->nkeys] = RSTRING_PTR(key);
    traits->lens[traits->nkeys] = RSTRING_LEN(key);
    traits->ttls[traits->nkeys] = NUM2ULONG(value);
    traits->nkeys++;
    return ST_CONTINUE;
}

    static long
cb_args_scan_keys(long argc, VALUE argv, bucket_t *bucket, struct key_traits *traits)
{
    VALUE key, *keys_ptr, opts, ttl, ext;
    long nn = 0, ii;
    time_t exp;

    traits->keys_ary = rb_ary_new();
    traits->quiet = bucket->quiet;
    traits->mgat = 0;

    if (argc > 0) {
        /* keys with custom options */
        opts = RARRAY_PTR(argv)[argc-1];
        exp = bucket->default_ttl;
        ext = Qfalse;
        if (argc > 1 && TYPE(opts) == T_HASH) {
            (void)rb_ary_pop(argv);
            if (RTEST(rb_funcall(opts, id_has_key_p, 1, sym_quiet))) {
                traits->quiet = RTEST(rb_hash_aref(opts, sym_quiet));
            }
            traits->force_format = rb_hash_aref(opts, sym_format);
            if (traits->force_format != Qnil) {
                Check_Type(traits->force_format, T_SYMBOL);
            }
            ext = rb_hash_aref(opts, sym_extended);
            ttl = rb_hash_aref(opts, sym_ttl);
            if (ttl != Qnil) {
                traits->explicit_ttl = 1;
                exp = NUM2ULONG(ttl);
            }
            nn = RARRAY_LEN(argv);
        } else {
            nn = argc;
        }
        if (nn < 1) {
            rb_raise(rb_eArgError, "must be at least one key");
        }
        keys_ptr = RARRAY_PTR(argv);
        traits->extended = RTEST(ext) ? 1 : 0;
        if (nn == 1 && TYPE(keys_ptr[0]) == T_HASH) {
            /* hash of key-ttl pairs */
            nn = RHASH_SIZE(keys_ptr[0]);
            traits->keys = xcalloc(nn, sizeof(char *));
            traits->lens = xcalloc(nn, sizeof(size_t));
            traits->explicit_ttl = 1;
            traits->mgat = 1;
            traits->ttls = xcalloc(nn, sizeof(time_t));
            rb_hash_foreach(keys_ptr[0], cb_extract_keys_i, (VALUE)traits);
        } else {
            /* the list of keys */
            traits->nkeys = nn;
            traits->keys = xcalloc(nn, sizeof(char *));
            traits->lens = xcalloc(nn, sizeof(size_t));
            traits->ttls = xcalloc(nn, sizeof(time_t));
            for (ii = 0; ii < nn; ii++) {
                key = unify_key(keys_ptr[ii]);
                rb_ary_push(traits->keys_ary, key);
                traits->keys[ii] = RSTRING_PTR(key);
                traits->lens[ii] = RSTRING_LEN(key);
                traits->ttls[ii] = exp;
            }
        }
    }

    return nn;
}

    static void
error_callback(libcouchbase_t handle, libcouchbase_error_t error, const char *errinfo)
{
    bucket_t *bucket = (bucket_t *)libcouchbase_get_cookie(handle);

    bucket->io->stop_event_loop(bucket->io);
    bucket->exception = cb_check_error(error, errinfo, Qnil);
}

    static void
storage_callback(libcouchbase_t handle, const void *cookie,
        libcouchbase_storage_t operation, libcouchbase_error_t error,
        const void *key, libcouchbase_size_t nkey, libcouchbase_cas_t cas)
{
    context_t *ctx = (context_t *)cookie;
    bucket_t *bucket = ctx->bucket;
    VALUE k, c, *rv = ctx->rv, exc, res;
    ID o;

    bucket->seqno--;

    k = rb_str_new((const char*)key, nkey);
    c = cas > 0 ? ULL2NUM(cas) : Qnil;
    switch(operation) {
        case LIBCOUCHBASE_ADD:
            o = sym_add;
            break;
        case LIBCOUCHBASE_REPLACE:
            o = sym_replace;
            break;
        case LIBCOUCHBASE_SET:
            o = sym_set;
            break;
        case LIBCOUCHBASE_APPEND:
            o = sym_append;
            break;
        case LIBCOUCHBASE_PREPEND:
            o = sym_prepend;
            break;
        default:
            o = Qnil;
    }
    exc = cb_check_error(error, "failed to store value", k);
    if (exc != Qnil) {
        rb_ivar_set(exc, id_iv_cas, c);
        rb_ivar_set(exc, id_iv_operation, o);
        if (NIL_P(ctx->exception)) {
            ctx->exception = exc;
            cb_gc_protect(bucket, ctx->exception);
        }
    }
    if (bucket->async) { /* asynchronous */
        if (ctx->proc != Qnil) {
            res = rb_class_new_instance(0, NULL, cResult);
            rb_ivar_set(res, id_iv_error, exc);
            rb_ivar_set(res, id_iv_key, k);
            rb_ivar_set(res, id_iv_operation, o);
            rb_ivar_set(res, id_iv_cas, c);
            cb_proc_call(ctx->proc, 1, res);
        }
    } else {             /* synchronous */
        *rv = c;
    }

    if (bucket->seqno == 0) {
        bucket->io->stop_event_loop(bucket->io);
        cb_gc_unprotect(bucket, ctx->proc);
    }
    (void)handle;
}

    static void
delete_callback(libcouchbase_t handle, const void *cookie,
        libcouchbase_error_t error, const void *key,
        libcouchbase_size_t nkey)
{
    context_t *ctx = (context_t *)cookie;
    bucket_t *bucket = ctx->bucket;
    VALUE k, *rv = ctx->rv, exc = Qnil, res;

    bucket->seqno--;

    k = rb_str_new((const char*)key, nkey);
    if (error != LIBCOUCHBASE_KEY_ENOENT || !ctx->quiet) {
        exc = cb_check_error(error, "failed to remove value", k);
        if (exc != Qnil) {
            rb_ivar_set(exc, id_iv_operation, sym_delete);
            if (NIL_P(ctx->exception)) {
                ctx->exception = exc;
                cb_gc_protect(bucket, ctx->exception);
            }
        }
    }
    if (bucket->async) {    /* asynchronous */
        if (ctx->proc != Qnil) {
            res = rb_class_new_instance(0, NULL, cResult);
            rb_ivar_set(res, id_iv_error, exc);
            rb_ivar_set(res, id_iv_operation, sym_delete);
            rb_ivar_set(res, id_iv_key, k);
            cb_proc_call(ctx->proc, 1, res);
        }
    } else {                /* synchronous */
        *rv = (error == LIBCOUCHBASE_SUCCESS) ? Qtrue : Qfalse;
    }
    if (bucket->seqno == 0) {
        bucket->io->stop_event_loop(bucket->io);
        cb_gc_unprotect(bucket, ctx->proc);
    }
    (void)handle;
}

    static void
get_callback(libcouchbase_t handle, const void *cookie,
        libcouchbase_error_t error, const void *key,
        libcouchbase_size_t nkey, const void *bytes,
        libcouchbase_size_t nbytes, libcouchbase_uint32_t flags,
        libcouchbase_cas_t cas)
{
    context_t *ctx = (context_t *)cookie;
    bucket_t *bucket = ctx->bucket;
    VALUE k, v, f, c, *rv = ctx->rv, exc = Qnil, res;

    bucket->seqno--;

    k = rb_str_new((const char*)key, nkey);
    if (error != LIBCOUCHBASE_KEY_ENOENT || !ctx->quiet) {
        exc = cb_check_error(error, "failed to get value", k);
        if (exc != Qnil) {
            rb_ivar_set(exc, id_iv_operation, sym_get);
            if (NIL_P(ctx->exception)) {
                ctx->exception = exc;
                cb_gc_protect(bucket, ctx->exception);
            }
        }
    }

    f = ULONG2NUM(flags);
    c = ULL2NUM(cas);
    v = Qnil;
    if (nbytes != 0) {
        v = decode_value(rb_str_new((const char*)bytes, nbytes), flags, ctx->force_format);
        if (v == Qundef) {
            if (ctx->exception != Qnil) {
                cb_gc_unprotect(bucket, ctx->exception);
            }
            ctx->exception = rb_exc_new2(eValueFormatError, "unable to convert value");
            rb_ivar_set(ctx->exception, id_iv_operation, sym_get);
            rb_ivar_set(ctx->exception, id_iv_key, k);
            cb_gc_protect(bucket, ctx->exception);
        }
    } else if (flags_get_format(flags) == sym_plain) {
        v = rb_str_new2("");
    }
    if (bucket->async) { /* asynchronous */
        if (ctx->proc != Qnil) {
            res = rb_class_new_instance(0, NULL, cResult);
            rb_ivar_set(res, id_iv_error, exc);
            rb_ivar_set(res, id_iv_operation, sym_get);
            rb_ivar_set(res, id_iv_key, k);
            rb_ivar_set(res, id_iv_value, v);
            rb_ivar_set(res, id_iv_flags, f);
            rb_ivar_set(res, id_iv_cas, c);
            cb_proc_call(ctx->proc, 1, res);
        }
    } else {                /* synchronous */
        if (NIL_P(exc) && error != LIBCOUCHBASE_KEY_ENOENT) {
            if (ctx->extended) {
                rb_hash_aset(*rv, k, rb_ary_new3(3, v, f, c));
            } else {
                rb_hash_aset(*rv, k, v);
            }
        }
    }

    if (bucket->seqno == 0) {
        bucket->io->stop_event_loop(bucket->io);
        cb_gc_unprotect(bucket, ctx->proc);
    }
    (void)handle;
}

    static void
flush_callback(libcouchbase_t handle, const void* cookie,
        const char* authority, libcouchbase_error_t error)
{
    context_t *ctx = (context_t *)cookie;
    bucket_t *bucket = ctx->bucket;
    VALUE node, success = Qtrue, *rv = ctx->rv, exc, res;

    node = authority ? rb_str_new2(authority) : Qnil;
    exc = cb_check_error(error, "failed to flush bucket", node);
    if (exc != Qnil) {
        rb_ivar_set(exc, id_iv_operation, sym_flush);
        if (NIL_P(ctx->exception)) {
            ctx->exception = exc;
            cb_gc_protect(bucket, ctx->exception);
        }
        success = Qfalse;
    }

    if (authority) {
        if (bucket->async) {    /* asynchronous */
            if (ctx->proc != Qnil) {
                res = rb_class_new_instance(0, NULL, cResult);
                rb_ivar_set(res, id_iv_error, exc);
                rb_ivar_set(res, id_iv_operation, sym_flush);
                rb_ivar_set(res, id_iv_node, node);
                cb_proc_call(ctx->proc, 1, res);
            }
        } else {                /* synchronous */
            if (RTEST(*rv)) {
                /* rewrite status for positive values only */
                *rv = success;
            }
        }
    } else {
        bucket->seqno--;
        if (bucket->seqno == 0) {
            bucket->io->stop_event_loop(bucket->io);
            cb_gc_unprotect(bucket, ctx->proc);
        }
    }

    (void)handle;
}

    static void
version_callback(libcouchbase_t handle, const void *cookie,
        const char *authority, libcouchbase_error_t error,
        const char *bytes, libcouchbase_size_t nbytes)
{
    context_t *ctx = (context_t *)cookie;
    bucket_t *bucket = ctx->bucket;
    VALUE node, v, *rv = ctx->rv, exc, res;

    node = authority ? rb_str_new2(authority) : Qnil;
    exc = cb_check_error(error, "failed to get version", node);
    if (exc != Qnil) {
        rb_ivar_set(exc, id_iv_operation, sym_flush);
        if (NIL_P(ctx->exception)) {
            ctx->exception = exc;
            cb_gc_protect(bucket, ctx->exception);
        }
    }

    if (authority) {
        v = rb_str_new((const char*)bytes, nbytes);
        if (bucket->async) {    /* asynchronous */
            if (ctx->proc != Qnil) {
                res = rb_class_new_instance(0, NULL, cResult);
                rb_ivar_set(res, id_iv_error, exc);
                rb_ivar_set(res, id_iv_operation, sym_version);
                rb_ivar_set(res, id_iv_node, node);
                rb_ivar_set(res, id_iv_value, v);
                cb_proc_call(ctx->proc, 1, res);
            }
        } else {                /* synchronous */
            if (NIL_P(exc)) {
                rb_hash_aset(*rv, node, v);
            }
        }
    } else {
        bucket->seqno--;
        if (bucket->seqno == 0) {
            bucket->io->stop_event_loop(bucket->io);
            cb_gc_unprotect(bucket, ctx->proc);
        }
    }

    (void)handle;
}

    static void
stat_callback(libcouchbase_t handle, const void* cookie,
        const char* authority, libcouchbase_error_t error, const void* key,
        libcouchbase_size_t nkey, const void* bytes,
        libcouchbase_size_t nbytes)
{
    context_t *ctx = (context_t *)cookie;
    bucket_t *bucket = ctx->bucket;
    VALUE stats, node, k, v, *rv = ctx->rv, exc = Qnil, res;

    node = authority ? rb_str_new2(authority) : Qnil;
    exc = cb_check_error(error, "failed to fetch stats", node);
    if (exc != Qnil) {
        rb_ivar_set(exc, id_iv_operation, sym_stats);
        if (NIL_P(ctx->exception)) {
            ctx->exception = exc;
            cb_gc_protect(bucket, ctx->exception);
        }
    }
    if (authority) {
        k = rb_str_new((const char*)key, nkey);
        v = rb_str_new((const char*)bytes, nbytes);
        if (bucket->async) {    /* asynchronous */
            if (ctx->proc != Qnil) {
                res = rb_class_new_instance(0, NULL, cResult);
                rb_ivar_set(res, id_iv_error, exc);
                rb_ivar_set(res, id_iv_operation, sym_stats);
                rb_ivar_set(res, id_iv_node, node);
                rb_ivar_set(res, id_iv_key, k);
                rb_ivar_set(res, id_iv_value, v);
                cb_proc_call(ctx->proc, 1, res);
            }
        } else {                /* synchronous */
            if (NIL_P(exc)) {
                stats = rb_hash_aref(*rv, k);
                if (NIL_P(stats)) {
                    stats = rb_hash_new();
                    rb_hash_aset(*rv, k, stats);
                }
                rb_hash_aset(stats, node, v);
            }
        }
    } else {
        bucket->seqno--;
        if (bucket->seqno == 0) {
            bucket->io->stop_event_loop(bucket->io);
            cb_gc_unprotect(bucket, ctx->proc);
        }
    }
    (void)handle;
}

    static void
touch_callback(libcouchbase_t handle, const void *cookie,
        libcouchbase_error_t error, const void *key,
        libcouchbase_size_t nkey)
{
    context_t *ctx = (context_t *)cookie;
    bucket_t *bucket = ctx->bucket;
    VALUE k, success, *rv = ctx->rv, exc = Qnil, res;

    bucket->seqno--;
    k = rb_str_new((const char*)key, nkey);
    if (error != LIBCOUCHBASE_KEY_ENOENT || !ctx->quiet) {
        exc = cb_check_error(error, "failed to touch value", k);
        if (exc != Qnil) {
            rb_ivar_set(exc, id_iv_operation, sym_touch);
            if (NIL_P(ctx->exception)) {
                ctx->exception = exc;
                cb_gc_protect(bucket, ctx->exception);
            }
        }
    }

    if (bucket->async) {    /* asynchronous */
        if (ctx->proc != Qnil) {
            res = rb_class_new_instance(0, NULL, cResult);
            rb_ivar_set(res, id_iv_error, exc);
            rb_ivar_set(res, id_iv_operation, sym_touch);
            rb_ivar_set(res, id_iv_key, k);
            cb_proc_call(ctx->proc, 1, res);
        }
    } else {                /* synchronous */
        if (NIL_P(exc)) {
            success = (error == LIBCOUCHBASE_KEY_ENOENT) ? Qfalse : Qtrue;
            rb_hash_aset(*rv, k, success);
        }
    }
    if (bucket->seqno == 0) {
        bucket->io->stop_event_loop(bucket->io);
        cb_gc_unprotect(bucket, ctx->proc);
    }
    (void)handle;
}

    static void
arithmetic_callback(libcouchbase_t handle, const void *cookie,
        libcouchbase_error_t error, const void *key,
        libcouchbase_size_t nkey, libcouchbase_uint64_t value,
        libcouchbase_cas_t cas)
{
    context_t *ctx = (context_t *)cookie;
    bucket_t *bucket = ctx->bucket;
    VALUE c, k, v, *rv = ctx->rv, exc, res;
    ID o;

    bucket->seqno--;

    k = rb_str_new((const char*)key, nkey);
    c = cas > 0 ? ULL2NUM(cas) : Qnil;
    o = ctx->arithm > 0 ? sym_increment : sym_decrement;
    exc = cb_check_error(error, "failed to perform arithmetic operation", k);
    if (exc != Qnil) {
        rb_ivar_set(exc, id_iv_cas, c);
        rb_ivar_set(exc, id_iv_operation, o);
        if (bucket->async) {
            if (bucket->on_error_proc != Qnil) {
                cb_proc_call(bucket->on_error_proc, 3, o, k, exc);
            } else {
                if (NIL_P(bucket->exception)) {
                    bucket->exception = exc;
                }
            }
        }
        if (NIL_P(ctx->exception)) {
            ctx->exception = exc;
            cb_gc_protect(bucket, ctx->exception);
        }
    }
    v = ULL2NUM(value);
    if (bucket->async) {    /* asynchronous */
        if (ctx->proc != Qnil) {
            res = rb_class_new_instance(0, NULL, cResult);
            rb_ivar_set(res, id_iv_error, exc);
            rb_ivar_set(res, id_iv_operation, o);
            rb_ivar_set(res, id_iv_key, k);
            rb_ivar_set(res, id_iv_value, v);
            rb_ivar_set(res, id_iv_cas, c);
            cb_proc_call(ctx->proc, 1, res);
        }
    } else {                /* synchronous */
        if (NIL_P(exc)) {
            if (ctx->extended) {
                *rv = rb_ary_new3(2, v, c);
            } else {
                *rv = v;
            }
        }
    }
    if (bucket->seqno == 0) {
        bucket->io->stop_event_loop(bucket->io);
        cb_gc_unprotect(bucket, ctx->proc);
    }
    (void)handle;
}

    static int
cb_first_value_i(VALUE key, VALUE value, VALUE arg)
{
    VALUE *val = (VALUE *)arg;

    *val = value;
    (void)key;
    return ST_STOP;
}

/*
 * @private
 * @return [Fixnum] number of scheduled operations
 */
    static VALUE
cb_bucket_seqno(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);

    return LONG2FIX(bucket->seqno);
}

    void
cb_bucket_free(void *ptr)
{
    bucket_t *bucket = ptr;

    if (bucket) {
        if (bucket->handle) {
            libcouchbase_destroy(bucket->handle);
        }
        xfree(bucket->authority);
        xfree(bucket->hostname);
        xfree(bucket->pool);
        xfree(bucket->bucket);
        xfree(bucket->username);
        xfree(bucket->password);
        xfree(bucket);
    }
}

    void
cb_bucket_mark(void *ptr)
{
    bucket_t *bucket = ptr;

    if (bucket) {
        rb_gc_mark(bucket->exception);
        rb_gc_mark(bucket->on_error_proc);
        rb_gc_mark(bucket->object_space);
    }
}

    static void
do_scan_connection_options(bucket_t *bucket, int argc, VALUE *argv)
{
    VALUE uri, opts, arg;
    size_t len;

    if (rb_scan_args(argc, argv, "02", &uri, &opts) > 0) {
        if (TYPE(uri) == T_HASH && argc == 1) {
            opts = uri;
            uri = Qnil;
        }
        if (uri != Qnil) {
            const char path_re[] = "^(/pools/([A-Za-z0-9_.-]+)(/buckets/([A-Za-z0-9_.-]+))?)?";
            VALUE match, uri_obj, re;

            Check_Type(uri, T_STRING);
            uri_obj = rb_funcall(mURI, id_parse, 1, uri);

            arg = rb_funcall(uri_obj, id_scheme, 0);
            if (arg == Qnil || rb_str_cmp(arg, rb_str_new2("http"))) {
                rb_raise(rb_eArgError, "invalid URI: invalid scheme");
            }

            arg = rb_funcall(uri_obj, id_user, 0);
            if (arg != Qnil) {
                xfree(bucket->username);
                bucket->username = strdup(RSTRING_PTR(arg));
                if (bucket->username == NULL) {
                    rb_raise(eNoMemoryError, "failed to allocate memory for Bucket");
                }
            }

            arg = rb_funcall(uri_obj, id_password, 0);
            if (arg != Qnil) {
                xfree(bucket->password);
                bucket->password = strdup(RSTRING_PTR(arg));
                if (bucket->password == NULL) {
                    rb_raise(eNoMemoryError, "failed to allocate memory for Bucket");
                }
            }
            arg = rb_funcall(uri_obj, id_host, 0);
            if (arg != Qnil) {
                xfree(bucket->hostname);
                bucket->hostname = strdup(RSTRING_PTR(arg));
                if (bucket->hostname == NULL) {
                    rb_raise(eNoMemoryError, "failed to allocate memory for Bucket");
                }
            } else {
                rb_raise(rb_eArgError, "invalid URI: missing hostname");
            }

            arg = rb_funcall(uri_obj, id_port, 0);
            bucket->port = NIL_P(arg) ? 8091 : (uint16_t)NUM2UINT(arg);

            arg = rb_funcall(uri_obj, id_path, 0);
            re = rb_reg_new(path_re, sizeof(path_re) - 1, 0);
            match = rb_funcall(re, id_match, 1, arg);
            arg = rb_reg_nth_match(2, match);
            xfree(bucket->pool);
            bucket->pool = strdup(NIL_P(arg) ? "default" : RSTRING_PTR(arg));
            arg = rb_reg_nth_match(4, match);
            xfree(bucket->bucket);
            bucket->bucket = strdup(NIL_P(arg) ? "default" : RSTRING_PTR(arg));
        }
        if (TYPE(opts) == T_HASH) {
            arg = rb_hash_aref(opts, sym_node_list);
            if (arg != Qnil) {
                VALUE tt;
                xfree(bucket->node_list);
                Check_Type(arg, T_ARRAY);
                tt = rb_ary_join(arg, rb_str_new2(";"));
                bucket->node_list = strdup(StringValueCStr(tt));
            }

            arg = rb_hash_aref(opts, sym_hostname);
            if (arg != Qnil) {
                if (bucket->hostname) {
                    xfree(bucket->hostname);
                }
                bucket->hostname = strdup(StringValueCStr(arg));
            }
            arg = rb_hash_aref(opts, sym_pool);
            if (arg != Qnil) {
                if (bucket->pool) {
                    xfree(bucket->pool);
                }
                bucket->pool = strdup(StringValueCStr(arg));
            }
            arg = rb_hash_aref(opts, sym_bucket);
            if (arg != Qnil) {
                if (bucket->bucket) {
                    xfree(bucket->bucket);
                }
                bucket->bucket = strdup(StringValueCStr(arg));
            }
            arg = rb_hash_aref(opts, sym_username);
            if (arg != Qnil) {
                if (bucket->username) {
                    xfree(bucket->username);
                }
                bucket->username = strdup(StringValueCStr(arg));
            }
            arg = rb_hash_aref(opts, sym_password);
            if (arg != Qnil) {
                if (bucket->password) {
                    xfree(bucket->password);
                }
                bucket->password = strdup(StringValueCStr(arg));
            }
            arg = rb_hash_aref(opts, sym_port);
            if (arg != Qnil) {
                bucket->port = (uint16_t)NUM2UINT(arg);
            }
            if (RTEST(rb_funcall(opts, id_has_key_p, 1, sym_quiet))) {
                bucket->quiet = RTEST(rb_hash_aref(opts, sym_quiet));
            }
            arg = rb_hash_aref(opts, sym_timeout);
            if (arg != Qnil) {
                bucket->timeout = (uint32_t)NUM2ULONG(arg);
            }
            arg = rb_hash_aref(opts, sym_default_ttl);
            if (arg != Qnil) {
                bucket->default_ttl = (uint32_t)NUM2ULONG(arg);
            }
            arg = rb_hash_aref(opts, sym_default_flags);
            if (arg != Qnil) {
                bucket->default_flags = (uint32_t)NUM2ULONG(arg);
            }
            arg = rb_hash_aref(opts, sym_default_format);
            if (arg != Qnil) {
                if (TYPE(arg) == T_FIXNUM) {
                    switch (FIX2INT(arg)) {
                        case FMT_DOCUMENT:
                            arg = sym_document;
                            break;
                        case FMT_MARSHAL:
                            arg = sym_marshal;
                            break;
                        case FMT_PLAIN:
                            arg = sym_plain;
                            break;
                    }
                }
                if (arg == sym_document || arg == sym_marshal || arg == sym_plain) {
                    bucket->default_format = arg;
                    bucket->default_flags = flags_set_format(bucket->default_flags, arg);
                }
            }
        } else {
            opts = Qnil;
        }
    }
    len = strlen(bucket->hostname) + 10;
    if (bucket->authority) {
        xfree(bucket->authority);
    }
    bucket->authority = xcalloc(len, sizeof(char));
    if (bucket->authority == NULL) {
        rb_raise(eNoMemoryError, "failed to allocate memory for Bucket");
    }
    snprintf(bucket->authority, len, "%s:%u", bucket->hostname, bucket->port);
}

    static void
do_connect(bucket_t *bucket)
{
    libcouchbase_error_t err;

    if (bucket->handle) {
        libcouchbase_destroy(bucket->handle);
        bucket->handle = NULL;
        bucket->io = NULL;
    }
    bucket->io = libcouchbase_create_io_ops(LIBCOUCHBASE_IO_OPS_DEFAULT, NULL, &err);
    if (bucket->io == NULL && err != LIBCOUCHBASE_SUCCESS) {
        rb_exc_raise(cb_check_error(err, "failed to create IO instance", Qnil));
    }
    //bucket->handle = libcouchbase_create(bucket->authority,
    //        bucket->username, bucket->password, bucket->bucket, bucket->io);
    bucket->handle = libcouchbase_create(bucket->node_list ? bucket->node_list : bucket->authority,
            bucket->username, bucket->password, bucket->bucket, bucket->io);
    if (bucket->handle == NULL) {
        rb_raise(eLibcouchbaseError, "failed to create libcouchbase instance");
    }
    libcouchbase_set_cookie(bucket->handle, bucket);
    (void)libcouchbase_set_error_callback(bucket->handle, error_callback);
    (void)libcouchbase_set_storage_callback(bucket->handle, storage_callback);
    (void)libcouchbase_set_get_callback(bucket->handle, get_callback);
    (void)libcouchbase_set_touch_callback(bucket->handle, touch_callback);
    (void)libcouchbase_set_remove_callback(bucket->handle, delete_callback);
    (void)libcouchbase_set_stat_callback(bucket->handle, stat_callback);
    (void)libcouchbase_set_flush_callback(bucket->handle, flush_callback);
    (void)libcouchbase_set_arithmetic_callback(bucket->handle, arithmetic_callback);
    (void)libcouchbase_set_version_callback(bucket->handle, version_callback);

    err = libcouchbase_connect(bucket->handle);
    if (err != LIBCOUCHBASE_SUCCESS) {
        libcouchbase_destroy(bucket->handle);
        bucket->handle = NULL;
        bucket->io = NULL;
        rb_exc_raise(cb_check_error(err, "failed to connect libcouchbase instance to server", Qnil));
    }
    bucket->exception = Qnil;
    libcouchbase_wait(bucket->handle);
    if (bucket->exception != Qnil) {
        libcouchbase_destroy(bucket->handle);
        bucket->handle = NULL;
        bucket->io = NULL;
        rb_exc_raise(bucket->exception);
    }

    if (bucket->timeout > 0) {
        libcouchbase_set_timeout(bucket->handle, bucket->timeout);
    } else {
        bucket->timeout = libcouchbase_get_timeout(bucket->handle);
    }
}

    static VALUE
cb_bucket_alloc(VALUE klass)
{
    VALUE obj;
    bucket_t *bucket;

    /* allocate new bucket struct and set it to zero */
    obj = Data_Make_Struct(klass, bucket_t, cb_bucket_mark, cb_bucket_free,
            bucket);
    return obj;
}

/*
 * Initialize new Bucket.
 *
 * @overload initialize(url, options = {})
 *   Initialize bucket using URI of the cluster and options. It is possible
 *   to override some parts of URI using the options keys (e.g. :host or
 *   :port)
 *
 *   @param [String] url The full URL of management API of the cluster.
 *   @param [Hash] options The options for connection. See options definition
 *     below.
 *
 * @overload initialize(options = {})
 *   Initialize bucket using options only.
 *
 *   @param [Hash] options The options for operation for connection
 *   @option options [String] :host ("localhost") the hostname or IP address
 *     of the node
 *   @option options [Fixnum] :port (8091) the port of the managemenent API
 *   @option options [String] :pool ("default") the pool name
 *   @option options [String] :bucket ("default") the bucket name
 *   @option options [Fixnum] :default_ttl (0) the TTL used by default during
 *     storing key-value pairs.
 *   @option options [Fixnum] :default_flags (0) the default flags.
 *   @option options [Symbol] :default_format (:document) the format, which
 *     will be used for values by default. Note that changing format will
 *     amend flags. (see {Bucket#default_format})
 *   @option options [String] :username (nil) the user name to connect to the
 *     cluster. Used to authenticate on management API.
 *   @option options [String] :password (nil) the password of the user.
 *   @option options [Boolean] :quiet (true) the flag controlling if raising
 *     exception when the client executes operations on unexising keys. If it
 *     is +true+ it will raise {Couchbase::Error::NotFound} exceptions. The
 *     default behaviour is to return +nil+ value silently (might be useful in
 *     Rails cache).
 *
 * @example Initialize connection using default options
 *   Couchbase.new
 *
 * @example Select custom bucket
 *   Couchbase.new(:bucket => 'foo')
 *   Couchbase.new('http://localhost:8091/pools/default/buckets/foo')
 *
 * @example Connect to protected bucket
 *   Couchbase.new(:bucket => 'protected', :username => 'protected', :password => 'secret')
 *   Couchbase.new('http://localhost:8091/pools/default/buckets/protected',
 *                 :username => 'protected', :password => 'secret')
 *
 * @return [Bucket]
 */
    static VALUE
cb_bucket_init(int argc, VALUE *argv, VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);

    bucket->exception = Qnil;
    bucket->hostname = strdup("localhost");
    bucket->port = 8091;
    bucket->pool = strdup("default");
    bucket->bucket = strdup("default");
    bucket->async = 0;
    bucket->quiet = 1;
    bucket->default_ttl = 0;
    bucket->default_flags = 0;
    bucket->default_format = sym_document;
    bucket->on_error_proc = Qnil;
    bucket->timeout = 0;
    bucket->object_space = rb_hash_new();
    bucket->node_list = NULL;

    do_scan_connection_options(bucket, argc, argv);
    do_connect(bucket);

    return self;
}

/*
 * Initialize copy
 *
 * Initializes copy of the object, used by {Couchbase::Bucket#dup}
 *
 * @param orig [Couchbase::Bucket] the source for copy
 *
 * @return [Couchbase::Bucket]
 */
static VALUE
cb_bucket_init_copy(VALUE copy, VALUE orig)
{
    bucket_t *copy_b;
    bucket_t *orig_b;

    if (copy == orig)
        return copy;

    if (TYPE(orig) != T_DATA || TYPE(copy) != T_DATA ||
            RDATA(orig)->dfree != (RUBY_DATA_FUNC)cb_bucket_free) {
        rb_raise(rb_eTypeError, "wrong argument type");
    }

    copy_b = DATA_PTR(copy);
    orig_b = DATA_PTR(orig);

    copy_b->port = orig_b->port;
    copy_b->authority = strdup(orig_b->authority);
    copy_b->hostname = strdup(orig_b->hostname);
    copy_b->pool = strdup(orig_b->pool);
    copy_b->bucket = strdup(orig_b->bucket);
    if (orig_b->username) {
        copy_b->username = strdup(orig_b->username);
    }
    if (orig_b->password) {
        copy_b->password = strdup(orig_b->password);
    }
    copy_b->async = orig_b->async;
    copy_b->quiet = orig_b->quiet;
    copy_b->seqno = orig_b->seqno;
    copy_b->default_format = orig_b->default_format;
    copy_b->default_flags = orig_b->default_flags;
    copy_b->default_ttl = orig_b->default_ttl;
    copy_b->timeout = orig_b->timeout;
    copy_b->exception = Qnil;
    if (orig_b->on_error_proc != Qnil) {
        copy_b->on_error_proc = rb_funcall(orig_b->on_error_proc, id_dup, 0);
    }

    do_connect(copy_b);

    return copy;
}

/*
 * Reconnect the bucket
 *
 * Reconnect the bucket using initial configuration with optional
 * redefinition.
 *
 * @overload reconnect(url, options = {})
 *  see {Bucket#initialize Bucket#initialize(url, options)}
 *
 * @overload reconnect(options = {})
 *  see {Bucket#initialize Bucket#initialize(options)}
 *
 *  @example reconnect with current parameters
 *    c.reconnect
 *
 *  @example reconnect the instance to another bucket
 *    c.reconnect(:bucket => 'new')
 */
    static VALUE
cb_bucket_reconnect(int argc, VALUE *argv, VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);

    do_scan_connection_options(bucket, argc, argv);
    do_connect(bucket);

    return self;
}

/* Document-method: connected?
 * Check whether the instance connected to the cluster.
 *
 * @return [Boolean] +true+ if the instance connected to the cluster
 */
    static VALUE
cb_bucket_connected_p(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    return bucket->handle ? Qtrue : Qfalse;
}

/* Document-method: async?
 * Check whether the connection asynchronous.
 *
 * By default all operations are synchronous and block waiting for
 * results, but you can make them asynchronous and run event loop
 * explicitly. (see {Bucket#run})
 *
 * @example Return value of #get operation depending on async flag
 *   connection = Connection.new
 *   connection.async?      #=> false
 *
 *   connection.run do |conn|
 *     conn.async?          #=> true
 *   end
 *
 * @return [Boolean] +true+ if the connection if asynchronous
 *
 * @see Bucket#run
 */
    static VALUE
cb_bucket_async_p(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    return bucket->async ? Qtrue : Qfalse;
}

    static VALUE
cb_bucket_quiet_get(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    return bucket->quiet ? Qtrue : Qfalse;
}

    static VALUE
cb_bucket_quiet_set(VALUE self, VALUE val)
{
    bucket_t *bucket = DATA_PTR(self);
    VALUE new;

    bucket->quiet = RTEST(val);
    new = bucket->quiet ? Qtrue : Qfalse;
    return new;
}

    static VALUE
cb_bucket_default_flags_get(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    return ULONG2NUM(bucket->default_flags);
}

    static VALUE
cb_bucket_default_flags_set(VALUE self, VALUE val)
{
    bucket_t *bucket = DATA_PTR(self);

    bucket->default_flags = (uint32_t)NUM2ULONG(val);
    bucket->default_format = flags_get_format(bucket->default_flags);
    return val;
}

    static VALUE
cb_bucket_default_format_get(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    return bucket->default_format;
}

    static VALUE
cb_bucket_default_format_set(VALUE self, VALUE val)
{
    bucket_t *bucket = DATA_PTR(self);

    if (TYPE(val) == T_FIXNUM) {
        switch (FIX2INT(val)) {
            case FMT_DOCUMENT:
                val = sym_document;
                break;
            case FMT_MARSHAL:
                val = sym_marshal;
                break;
            case FMT_PLAIN:
                val = sym_plain;
                break;
        }
    }
    if (val == sym_document || val == sym_marshal || val == sym_plain) {
        bucket->default_format = val;
        bucket->default_flags = flags_set_format(bucket->default_flags, val);
    }

    return val;
}

    static VALUE
cb_bucket_on_error_set(VALUE self, VALUE val)
{
    bucket_t *bucket = DATA_PTR(self);

    if (rb_respond_to(val, id_call)) {
        bucket->on_error_proc = val;
    } else {
        bucket->on_error_proc = Qnil;
    }

    return bucket->on_error_proc;
}

    static VALUE
cb_bucket_on_error_get(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);

    if (rb_block_given_p()) {
        return cb_bucket_on_error_set(self, rb_block_proc());
    } else {
        return bucket->on_error_proc;
    }
}

    static VALUE
cb_bucket_timeout_get(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    return ULONG2NUM(bucket->timeout);
}

    static VALUE
cb_bucket_timeout_set(VALUE self, VALUE val)
{
    bucket_t *bucket = DATA_PTR(self);
    VALUE tmval;

    bucket->timeout = (uint32_t)NUM2ULONG(val);
    libcouchbase_set_timeout(bucket->handle, bucket->timeout);
    tmval = ULONG2NUM(bucket->timeout);

    return tmval;
}

/* Document-method: hostname
 * @return [String] the host name of the management interface (default: "localhost")
 */
    static VALUE
cb_bucket_hostname_get(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    if (bucket->handle) {
        if (bucket->hostname) {
            xfree(bucket->hostname);
            bucket->hostname = NULL;
        }
        bucket->hostname = strdup(libcouchbase_get_host(bucket->handle));
        if (bucket->hostname == NULL) {
            rb_raise(eNoMemoryError, "failed to allocate memory for Bucket");
        }
    }
    return rb_str_new2(bucket->hostname);
}

/* Document-method: port
 * @return [Fixnum] the port number of the management interface (default: 8091)
 */
    static VALUE
cb_bucket_port_get(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    if (bucket->handle) {
        bucket->port = atoi(libcouchbase_get_port(bucket->handle));
    }
    return UINT2NUM(bucket->port);
}

/* Document-method: authority
 * @return [String] host with port
 */
    static VALUE
cb_bucket_authority_get(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    size_t len;

    (void)cb_bucket_hostname_get(self);
    (void)cb_bucket_port_get(self);
    len = strlen(bucket->hostname) + 10;
    bucket->authority = xcalloc(len, sizeof(char));
    if (bucket->authority == NULL) {
        rb_raise(eNoMemoryError, "failed to allocate memory for Bucket");
    }
    snprintf(bucket->authority, len, "%s:%u", bucket->hostname, bucket->port);
    return rb_str_new2(bucket->authority);
}

/* Document-method: bucket
 * @return [String] the bucket name
 */
    static VALUE
cb_bucket_bucket_get(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    return rb_str_new2(bucket->bucket);
}

/* Document-method: pool
 * @return [String] the pool name (usually "default")
 */
    static VALUE
cb_bucket_pool_get(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    return rb_str_new2(bucket->pool);
}

/* Document-method: username
 * @return [String] the username for protected buckets (usually matches
 *   the bucket name)
 */
    static VALUE
cb_bucket_username_get(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    return rb_str_new2(bucket->username);
}

/* Document-method: password
 * @return [String] the password for protected buckets
 */
    static VALUE
cb_bucket_password_get(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    return rb_str_new2(bucket->password);
}

/* Document-method: url
 * @return [String] the address of the cluster management interface
 */
    static VALUE
cb_bucket_url_get(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    VALUE str;

    (void)cb_bucket_authority_get(self);
    str = rb_str_buf_new2("http://");
    rb_str_buf_cat2(str, bucket->authority);
    rb_str_buf_cat2(str, "/pools/");
    rb_str_buf_cat2(str, bucket->pool);
    rb_str_buf_cat2(str, "/buckets/");
    rb_str_buf_cat2(str, bucket->bucket);
    rb_str_buf_cat2(str, "/");
    return str;
}

/*
 * Returns a string containing a human-readable representation of the
 * Bucket.
 *
 * @return [String]
 */
    static VALUE
cb_bucket_inspect(VALUE self)
{
    VALUE str;
    bucket_t *bucket = DATA_PTR(self);
    char buf[200];

    str = rb_str_buf_new2("#<");
    rb_str_buf_cat2(str, rb_obj_classname(self));
    snprintf(buf, 25, ":%p \"", (void *)self);
    (void)cb_bucket_authority_get(self);
    rb_str_buf_cat2(str, buf);
    rb_str_buf_cat2(str, "http://");
    rb_str_buf_cat2(str, bucket->authority);
    rb_str_buf_cat2(str, "/pools/");
    rb_str_buf_cat2(str, bucket->pool);
    rb_str_buf_cat2(str, "/buckets/");
    rb_str_buf_cat2(str, bucket->bucket);
    rb_str_buf_cat2(str, "/");
    snprintf(buf, 150, "\" default_format=:%s, default_flags=0x%x, quiet=%s, connected=%s, timeout=%u>",
            rb_id2name(SYM2ID(bucket->default_format)),
            bucket->default_flags,
            bucket->quiet ? "true" : "false",
            bucket->handle ? "true" : "false",
            bucket->timeout);
    rb_str_buf_cat2(str, buf);

    return str;
}

/*
 * Delete the specified key
 *
 * @overload delete(key, options = {})
 *   @param key [String, Symbol] Key used to reference the value.
 *   @param options [Hash] Options for operation.
 *   @option options [Boolean] :quiet (self.quiet) If set to +true+, the
 *     operation won't raise error for missing key, it will return +nil+.
 *     Otherwise it will raise error in synchronous mode. In asynchronous
 *     mode this option ignored.
 *   @option options [Fixnum] :cas The CAS value for an object. This value
 *     created on the server and is guaranteed to be unique for each value of
 *     a given key. This value is used to provide simple optimistic
 *     concurrency control when multiple clients or threads try to
 *     update/delete an item simultaneously.
 *
 *   @raise [Couchbase::Error::Connect] if connection closed (see {Bucket#reconnect})
 *   @raise [ArgumentError] when passing the block in synchronous mode
 *   @raise [Couchbase::Error::KeyExists] on CAS mismatch
 *   @raise [Couchbase::Error::NotFound] if key is missing in verbose mode
 *
 *   @example Delete the key in quiet mode (default)
 *     c.set("foo", "bar")
 *     c.delete("foo")        #=> true
 *     c.delete("foo")        #=> false
 *
 *   @example Delete the key verbosely
 *     c.set("foo", "bar")
 *     c.delete("foo", :quiet => false)   #=> true
 *     c.delete("foo", :quiet => false)   #=> will raise Couchbase::Error::NotFound
 *
 *   @example Delete the key with version check
 *     ver = c.set("foo", "bar")          #=> 5992859822302167040
 *     c.delete("foo", :cas => 123456)    #=> will raise Couchbase::Error::KeyExists
 *     c.delete("foo", :cas => ver)       #=> true
 */
    static VALUE
cb_bucket_delete(int argc, VALUE *argv, VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    context_t *ctx;
    VALUE k, c, rv, proc, exc, opts;
    char *key;
    size_t nkey;
    libcouchbase_cas_t cas = 0;
    libcouchbase_error_t err;
    long seqno;

    if (bucket->handle == NULL) {
        rb_raise(eConnectError, "closed connection");
    }
    rb_scan_args(argc, argv, "11&", &k, &opts, &proc);
    if (!bucket->async && proc != Qnil) {
        rb_raise(rb_eArgError, "synchronous mode doesn't support callbacks");
    }
    k = unify_key(k);
    key = RSTRING_PTR(k);
    nkey = RSTRING_LEN(k);
    ctx = xcalloc(1, sizeof(context_t));
    ctx->quiet = bucket->quiet;
    if (ctx == NULL) {
        rb_raise(eNoMemoryError, "failed to allocate memory for context");
    }
    if (opts != Qnil) {
        if (TYPE(opts) == T_BIGNUM || TYPE(opts) == T_FIXNUM) {
            cas = NUM2ULL(opts);
        } else {
            Check_Type(opts, T_HASH);
            if ((c = rb_hash_aref(opts, sym_cas)) != Qnil) {
                cas = NUM2ULL(c);
            }
            if (RTEST(rb_funcall(opts, id_has_key_p, 1, sym_quiet))) {
                ctx->quiet = RTEST(rb_hash_aref(opts, sym_quiet));
            }
        }
    }
    ctx->proc = proc;
    cb_gc_protect(bucket, ctx->proc);
    rv = rb_ary_new();
    ctx->rv = &rv;
    ctx->bucket = bucket;
    ctx->exception = Qnil;
    seqno = bucket->seqno;
    bucket->seqno++;
    err = libcouchbase_remove(bucket->handle, (const void *)ctx,
            (const void *)key, nkey, cas);
    exc = cb_check_error(err, "failed to schedule delete request", Qnil);
    if (exc != Qnil) {
        xfree(ctx);
        rb_exc_raise(exc);
    }
    if (bucket->async) {
        return Qnil;
    } else {
        if (bucket->seqno - seqno > 0) {
            /* we have some operations pending */
            bucket->io->run_event_loop(bucket->io);
        }
        exc = ctx->exception;
        xfree(ctx);
        if (exc != Qnil) {
            cb_gc_unprotect(bucket, exc);
            rb_exc_raise(exc);
        }
        return rv;
    }
}

    static inline VALUE
cb_bucket_store(libcouchbase_storage_t cmd, int argc, VALUE *argv, VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    context_t *ctx;
    VALUE k, v, arg, opts, rv, proc, exc, fmt;
    char *key, *bytes;
    size_t nkey, nbytes;
    uint32_t flags;
    time_t exp = 0;
    libcouchbase_cas_t cas = 0;
    libcouchbase_error_t err;
    long seqno;

    if (bucket->handle == NULL) {
        rb_raise(eConnectError, "closed connection");
    }
    rb_scan_args(argc, argv, "21&", &k, &v, &opts, &proc);
    if (!bucket->async && proc != Qnil) {
        rb_raise(rb_eArgError, "synchronous mode doesn't support callbacks");
    }
    k = unify_key(k);
    flags = bucket->default_flags;
    if (opts != Qnil) {
        Check_Type(opts, T_HASH);
        arg = rb_hash_aref(opts, sym_flags);
        if (arg != Qnil) {
            flags = (uint32_t)NUM2ULONG(arg);
        }
        arg = rb_hash_aref(opts, sym_ttl);
        if (arg != Qnil) {
            exp = NUM2ULONG(arg);
        }
        arg = rb_hash_aref(opts, sym_cas);
        if (arg != Qnil) {
            cas = NUM2ULL(arg);
        }
        fmt = rb_hash_aref(opts, sym_format);
        if (fmt != Qnil) { /* rewrite format bits */
            flags = flags_set_format(flags, fmt);
        }
    }
    key = RSTRING_PTR(k);
    nkey = RSTRING_LEN(k);
    v = encode_value(v, flags);
    if (v == Qundef) {
        rb_raise(eValueFormatError,
                "unable to convert value for key '%s'", key);
    }
    bytes = RSTRING_PTR(v);
    nbytes = RSTRING_LEN(v);
    ctx = xcalloc(1, sizeof(context_t));
    if (ctx == NULL) {
        rb_raise(eNoMemoryError, "failed to allocate memory for context");
    }
    rv = Qnil;
    ctx->rv = &rv;
    ctx->bucket = bucket;
    ctx->proc = proc;
    cb_gc_protect(bucket, ctx->proc);
    ctx->exception = Qnil;
    seqno = bucket->seqno;
    bucket->seqno++;
    err = libcouchbase_store(bucket->handle, (const void *)ctx, cmd,
            (const void *)key, nkey, bytes, nbytes, flags, exp, cas);
    exc = cb_check_error(err, "failed to schedule set request", Qnil);
    if (exc != Qnil) {
        xfree(ctx);
        rb_exc_raise(exc);
    }
    if (bucket->async) {
        return Qnil;
    } else {
        if (bucket->seqno - seqno > 0) {
            /* we have some operations pending */
            bucket->io->run_event_loop(bucket->io);
        }
        exc = ctx->exception;
        xfree(ctx);
        if (exc != Qnil) {
            cb_gc_unprotect(bucket, exc);
            rb_exc_raise(exc);
        }
        if (bucket->exception != Qnil) {
            rb_exc_raise(bucket->exception);
        }
        return rv;
    }
}

    static inline VALUE
cb_bucket_arithmetic(int sign, int argc, VALUE *argv, VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    context_t *ctx;
    VALUE k, d, arg, opts, rv, proc, exc;
    char *key;
    size_t nkey;
    time_t exp;
    uint64_t delta = 0, initial = 0;
    int create = 0;
    libcouchbase_error_t err;
    long seqno;

    if (bucket->handle == NULL) {
        rb_raise(eConnectError, "closed connection");
    }
    rb_scan_args(argc, argv, "12&", &k, &d, &opts, &proc);
    if (!bucket->async && proc != Qnil) {
        rb_raise(rb_eArgError, "synchronous mode doesn't support callbacks");
    }
    k = unify_key(k);
    ctx = xcalloc(1, sizeof(context_t));
    if (ctx == NULL) {
        rb_raise(eNoMemoryError, "failed to allocate memory for context");
    }
    if (argc == 2 && TYPE(d) == T_HASH) {
        opts = d;
        d = Qnil;
    }
    exp = bucket->default_ttl;
    if (opts != Qnil) {
        Check_Type(opts, T_HASH);
        create = RTEST(rb_hash_aref(opts, sym_create));
        ctx->extended = RTEST(rb_hash_aref(opts, sym_extended));
        arg = rb_hash_aref(opts, sym_ttl);
        if (arg != Qnil) {
            exp = NUM2ULONG(arg);
        }
        arg = rb_hash_aref(opts, sym_initial);
        if (arg != Qnil) {
            initial = NUM2ULL(arg);
            create = 1;
        }
    }
    key = RSTRING_PTR(k);
    nkey = RSTRING_LEN(k);
    if (NIL_P(d)) {
        delta = 1 * sign;
    } else {
        delta = NUM2ULL(d) * sign;
    }
    rv = Qnil;
    ctx->rv = &rv;
    ctx->bucket = bucket;
    ctx->proc = proc;
    cb_gc_protect(bucket, ctx->proc);
    ctx->exception = Qnil;
    ctx->arithm = sign;
    seqno = bucket->seqno;
    bucket->seqno++;
    err = libcouchbase_arithmetic(bucket->handle, (const void *)ctx,
            (const void *)key, nkey, delta, exp, create, initial);
    exc = cb_check_error(err, "failed to schedule arithmetic request", k);
    if (exc != Qnil) {
        xfree(ctx);
        rb_exc_raise(exc);
    }
    if (bucket->async) {
        return Qnil;
    } else {
        if (bucket->seqno - seqno > 0) {
            /* we have some operations pending */
            bucket->io->run_event_loop(bucket->io);
        }
        exc = ctx->exception;
        xfree(ctx);
        if (exc != Qnil) {
            cb_gc_unprotect(bucket, exc);
            rb_exc_raise(exc);
        }
        return rv;
    }
}

/*
 * Increment the value of an existing numeric key
 *
 * The increment methods enable you to increase a given stored integer
 * value. These are the incremental equivalent of the decrement operations
 * and work on the same basis; updating the value of a key if it can be
 * parsed to an integer. The update operation occurs on the server and is
 * provided at the protocol level. This simplifies what would otherwise be a
 * two-stage get and set operation.
 *
 * @note that server values stored and transmitted as unsigned numbers,
 *   therefore if you try to store negative number and then increment or
 *   decrement it will cause overflow. (see "Integer overflow" example
 *   below)
 *
 * @overload incr(key, delta = 1, options = {})
 *   @param key [String, Symbol] Key used to reference the value.
 *   @param delta [Fixnum] Integer (up to 64 bits) value to increment
 *   @param options [Hash] Options for operation.
 *   @option options [Boolean] :create (false) If set to +true+, it will
 *     initialize the key with zero value and zero flags (use +:initial+
 *     option to set another initial value). Note: it won't increment the
 *     missing value.
 *   @option options [Fixnum] :initial (0) Integer (up to 64 bits) value for
 *     missing key initialization. This option imply +:create+ option is
 *     +true+.
 *   @option options [Fixnum] :ttl (self.default_ttl) Expiry time for key.
 *     Values larger than 30*24*60*60 seconds (30 days) are interpreted as
 *     absolute times (from the epoch). This option ignored for existent
 *     keys.
 *   @option options [Boolean] :extended (false) If set to +true+, the
 *     operation will return tuple +[value, cas]+, otherwise (by default) it
 *     returns just value.
 *
 *   @yieldparam ret [Result] the result of operation in asynchronous mode
 *     (valid attributes: +error+, +operation+, +key+, +value+, +cas+).
 *
 *   @return [Fixnum] the actual value of the key.
 *
 *   @raise [Couchbase::Error::NotFound] if key is missing and +:create+
 *     option isn't +true+.
 *
 *   @raise [Couchbase::Error::DeltaBadval] if the key contains non-numeric
 *     value
 *
 *   @raise [Couchbase::Error::Connect] if connection closed (see {Bucket#reconnect})
 *
 *   @raise [ArgumentError] when passing the block in synchronous mode
 *
 *   @example Increment key by one
 *     c.incr("foo")
 *
 *   @example Increment key by 50
 *     c.incr("foo", 50)
 *
 *   @example Increment key by one <b>OR</b> initialize with zero
 *     c.incr("foo", :create => true)   #=> will return old+1 or 0
 *
 *   @example Increment key by one <b>OR</b> initialize with three
 *     c.incr("foo", 50, :initial => 3) #=> will return old+50 or 3
 *
 *   @example Increment key and get its CAS value
 *     val, cas = c.incr("foo", :extended => true)
 *
 *   @example Integer overflow
 *     c.set("foo", -100)
 *     c.get("foo")           #=> -100
 *     c.incr("foo")          #=> 18446744073709551517
 *
 *   @example Asynchronous invocation
 *     c.run do
 *       c.incr("foo") do |ret|
 *         ret.operation   #=> :increment
 *         ret.success?    #=> true
 *         ret.key         #=> "foo"
 *         ret.value
 *         ret.cas
 *       end
 *     end
 *
 */
static VALUE
cb_bucket_incr(int argc, VALUE *argv, VALUE self)
{
    return cb_bucket_arithmetic(+1, argc, argv, self);
}

/*
 * Decrement the value of an existing numeric key
 *
 * The decrement methods reduce the value of a given key if the
 * corresponding value can be parsed to an integer value. These operations
 * are provided at a protocol level to eliminate the need to get, update,
 * and reset a simple integer value in the database. It supports the use of
 * an explicit offset value that will be used to reduce the stored value in
 * the database.
 *
 * @note that server values stored and transmitted as unsigned numbers,
 *   therefore if you try to decrement negative or zero key, you will always
 *   get zero.
 *
 * @overload decr(key, delta = 1, options = {})
 *   @param key [String, Symbol] Key used to reference the value.
 *   @param delta [Fixnum] Integer (up to 64 bits) value to decrement
 *   @param options [Hash] Options for operation.
 *   @option options [Boolean] :create (false) If set to +true+, it will
 *     initialize the key with zero value and zero flags (use +:initial+
 *     option to set another initial value). Note: it won't decrement the
 *     missing value.
 *   @option options [Fixnum] :initial (0) Integer (up to 64 bits) value for
 *     missing key initialization. This option imply +:create+ option is
 *     +true+.
 *   @option options [Fixnum] :ttl (self.default_ttl) Expiry time for key.
 *     Values larger than 30*24*60*60 seconds (30 days) are interpreted as
 *     absolute times (from the epoch). This option ignored for existent
 *     keys.
 *   @option options [Boolean] :extended (false) If set to +true+, the
 *     operation will return tuple +[value, cas]+, otherwise (by default) it
 *     returns just value.
 *
 *   @yieldparam ret [Result] the result of operation in asynchronous mode
 *     (valid attributes: +error+, +operation+, +key+, +value+, +cas+).
 *
 *   @return [Fixnum] the actual value of the key.
 *
 *   @raise [Couchbase::Error::NotFound] if key is missing and +:create+
 *     option isn't +true+.
 *
 *   @raise [Couchbase::Error::DeltaBadval] if the key contains non-numeric
 *     value
 *
 *   @raise [Couchbase::Error::Connect] if connection closed (see {Bucket#reconnect})
 *
 *   @raise [ArgumentError] when passing the block in synchronous mode
 *
 *   @example Decrement key by one
 *     c.decr("foo")
 *
 *   @example Decrement key by 50
 *     c.decr("foo", 50)
 *
 *   @example Decrement key by one <b>OR</b> initialize with zero
 *     c.decr("foo", :create => true)   #=> will return old-1 or 0
 *
 *   @example Decrement key by one <b>OR</b> initialize with three
 *     c.decr("foo", 50, :initial => 3) #=> will return old-50 or 3
 *
 *   @example Decrement key and get its CAS value
 *     val, cas = c.decr("foo", :extended => true)
 *
 *   @example Decrementing zero
 *     c.set("foo", 0)
 *     c.decrement("foo", 100500)   #=> 0
 *
 *   @example Decrementing negative value
 *     c.set("foo", -100)
 *     c.decrement("foo", 100500)   #=> 0
 *
 *   @example Asynchronous invocation
 *     c.run do
 *       c.decr("foo") do |ret|
 *         ret.operation   #=> :decrement
 *         ret.success?    #=> true
 *         ret.key         #=> "foo"
 *         ret.value
 *         ret.cas
 *       end
 *     end
 *
 */
static VALUE
cb_bucket_decr(int argc, VALUE *argv, VALUE self)
{
    return cb_bucket_arithmetic(-1, argc, argv, self);
}

/*
 * Obtain an object stored in Couchbase by given key.
 *
 * @overload get(*keys, options = {})
 *   @param keys [String, Symbol, Array] One or several keys to fetch
 *   @param options [Hash] Options for operation.
 *   @option options [Boolean] :extended (false) If set to +true+, the
 *     operation will return tuple +[value, flags, cas]+, otherwise (by
 *     default) it returns just value.
 *   @option options [Fixnum] :ttl (self.default_ttl) Expiry time for key.
 *     Values larger than 30*24*60*60 seconds (30 days) are interpreted as
 *     absolute times (from the epoch).
 *   @option options [Boolean] :quiet (self.quiet) If set to +true+, the
 *     operation won't raise error for missing key, it will return +nil+.
 *     Otherwise it will raise error in synchronous mode. In asynchronous
 *     mode this option ignored.
 *   @option options [Symbol] :format (nil) Explicitly choose the decoder
 *     for this key (+:plain+, +:document+, +:marshal+). See
 *     {Bucket#default_format}.
 *
 *   @yieldparam ret [Result] the result of operation in asynchronous mode
 *     (valid attributes: +error+, +operation+, +key+, +value+, +flags+,
 *     +cas+).
 *
 *   @return [Object, Array, Hash] the value(s) (or tuples in extended mode)
 *     assiciated with the key.
 *
 *   @raise [Couchbase::Error::NotFound] if the key is missing in the
 *     bucket.
 *
 *   @raise [Couchbase::Error::Connect] if connection closed (see {Bucket#reconnect})
 *
 *   @raise [ArgumentError] when passing the block in synchronous mode
 *
 *   @example Get single value in quite mode (the default)
 *     c.get("foo")     #=> the associated value or nil
 *
 *   @example Use alternative hash-like syntax
 *     c["foo"]         #=> the associated value or nil
 *
 *   @example Get single value in verbose mode
 *     c.get("missing-foo", :quiet => false)  #=> raises Couchbase::NotFound
 *
 *   @example Get and touch single value. The key won't be accessible after 10 seconds
 *     c.get("foo", :ttl => 10)
 *
 *   @example Extended get
 *     val, flags, cas = c.get("foo", :extended => true)
 *
 *   @example Get multiple keys
 *     c.get("foo", "bar", "baz")   #=> [val1, val2, val3]
 *
 *   @example Extended get multiple keys
 *     c.get("foo", "bar", :extended => true)
 *     #=> {"foo" => [val1, flags1, cas1], "bar" => [val2, flags2, cas2]}
 *
 *   @example Asynchronous get
 *     c.run do
 *       c.get("foo", "bar", "baz") do |res|
 *         ret.operation   #=> :get
 *         ret.success?    #=> true
 *         ret.key         #=> "foo", "bar" or "baz" in separate calls
 *         ret.value
 *         ret.flags
 *         ret.cas
 *       end
 *     end
 *
 * @overload get(keys, options = {})
 *   When the method receive hash map, it will behave like it receive list
 *   of keys (+keys.keys+), but also touch each key setting expiry time to
 *   the corresponding value. But unlike usual get this command always
 *   return hash map +{key => value}+ or +{key => [value, flags, cas]}+.
 *
 *   @param keys [Hash] Map key-ttl
 *   @param options [Hash] Options for operation. (see options definition
 *     above)
 *
 *   @return [Hash] the values (or tuples in extended mode) assiciated with
 *     the keys.
 *
 *   @example Get and touch multiple keys
 *     c.get("foo" => 10, "bar" => 20)   #=> {"foo" => val1, "bar" => val2}
 *
 *   @example Extended get and touch multiple keys
 *     c.get({"foo" => 10, "bar" => 20}, :extended => true)
 *     #=> {"foo" => [val1, flags1, cas1], "bar" => [val2, flags2, cas2]}
 */
    static VALUE
cb_bucket_get(int argc, VALUE *argv, VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    context_t *ctx;
    VALUE args, rv, proc, exc, keys;
    long nn;
    libcouchbase_error_t err;
    struct key_traits *traits;
    int extended, mgat;
    long seqno;

    if (bucket->handle == NULL) {
        rb_raise(eConnectError, "closed connection");
    }
    rb_scan_args(argc, argv, "0*&", &args, &proc);
    if (!bucket->async && proc != Qnil) {
        rb_raise(rb_eArgError, "synchronous mode doesn't support callbacks");
    }
    rb_funcall(args, id_flatten_bang, 0);
    traits = xcalloc(1, sizeof(struct key_traits));
    nn = cb_args_scan_keys(RARRAY_LEN(args), args, bucket, traits);
    ctx = xcalloc(1, sizeof(context_t));
    if (ctx == NULL) {
        rb_raise(eNoMemoryError, "failed to allocate memory for context");
    }
    mgat = traits->mgat;
    keys = traits->keys_ary;
    ctx->proc = proc;
    cb_gc_protect(bucket, ctx->proc);
    ctx->bucket = bucket;
    ctx->extended = traits->extended;
    ctx->quiet = traits->quiet;
    ctx->force_format = traits->force_format;
    rv = rb_hash_new();
    ctx->rv = &rv;
    ctx->exception = Qnil;
    seqno = bucket->seqno;
    bucket->seqno += nn;
    err = libcouchbase_mget(bucket->handle, (const void *)ctx,
            traits->nkeys, (const void * const *)traits->keys,
            traits->lens, (traits->explicit_ttl) ? traits->ttls : NULL);
    xfree(traits->keys);
    xfree(traits->lens);
    xfree(traits->ttls);
    xfree(traits);
    exc = cb_check_error(err, "failed to schedule get request", Qnil);
    if (exc != Qnil) {
        xfree(ctx);
        rb_exc_raise(exc);
    }
    if (bucket->async) {
        return Qnil;
    } else {
        if (bucket->seqno - seqno > 0) {
            /* we have some operations pending */
            bucket->io->run_event_loop(bucket->io);
        }
        exc = ctx->exception;
        extended = ctx->extended;
        xfree(ctx);
        if (exc != Qnil) {
            cb_gc_unprotect(bucket, exc);
            rb_exc_raise(exc);
        }
        if (bucket->exception != Qnil) {
            rb_exc_raise(bucket->exception);
        }
        if (mgat || (extended && nn > 1)) {
            return rv;  /* return as a hash {key => [value, flags, cas], ...} */
        }
        if (nn > 1) {
            long ii;
            VALUE *keys_ptr, ret;
            ret = rb_ary_new();
            keys_ptr = RARRAY_PTR(keys);
            for (ii = 0; ii < nn; ii++) {
                rb_ary_push(ret, rb_hash_aref(rv, keys_ptr[ii]));
            }
            return ret;  /* return as an array [value1, value2, ...] */
        } else {
            VALUE vv = Qnil;
            rb_hash_foreach(rv, cb_first_value_i, (VALUE)&vv);
            return vv;
        }
    }
}

/*
 * Update the expiry time of an item
 *
 * The +touch+ method allow you to update the expiration time on a given
 * key. This can be useful for situations where you want to prevent an item
 * from expiring without resetting the associated value. For example, for a
 * session database you might want to keep the session alive in the database
 * each time the user accesses a web page without explicitly updating the
 * session value, keeping the user's session active and available.
 *
 * @overload touch(key, options = {})
 *   @param key [String, Symbol] Key used to reference the value.
 *   @param options [Hash] Options for operation.
 *   @option options [Fixnum] :ttl (self.default_ttl) Expiry time for key.
 *     Values larger than 30*24*60*60 seconds (30 days) are interpreted as
 *     absolute times (from the epoch).
 *
 *   @yieldparam ret [Result] the result of operation in asynchronous mode
 *     (valid attributes: +error+, +operation+, +key+).
 *
 *   @return [Boolean] +true+ if the operation was successful and +false+
 *     otherwise.
 *
 *   @raise [Couchbase::Error::Connect] if connection closed (see {Bucket#reconnect})
 *
 *   @raise [ArgumentError] when passing the block in synchronous mode
 *
 *   @example Touch value using +default_ttl+
 *     c.touch("foo")
 *
 *   @example Touch value using custom TTL (10 seconds)
 *     c.touch("foo", :ttl => 10)
 *
 * @overload touch(keys)
 *   @param keys [Hash] The Hash where keys represent the keys in the
 *     database, values -- the expiry times for corresponding key. See
 *     description of +:ttl+ argument above for more information about TTL
 *     values.
 *
 *   @yieldparam ret [Result] the result of operation for each key in
 *     asynchronous mode (valid attributes: +error+, +operation+, +key+).
 *
 *   @return [Hash] Mapping keys to result of touch operation (+true+ if the
 *     operation was successful and +false+ otherwise)
 *
 *   @example Touch several values
 *     c.touch("foo" => 10, :bar => 20) #=> {"foo" => true, "bar" => true}
 *
 *   @example Touch several values in async mode
 *     c.run do
 *       c.touch("foo" => 10, :bar => 20) do |ret|
 *          ret.operation   #=> :touch
 *          ret.success?    #=> true
 *          ret.key         #=> "foo" and "bar" in separate calls
 *       end
 *     end
 *
 *   @example Touch single value
 *     c.touch("foo" => 10)             #=> true
 *
 */
   static VALUE
cb_bucket_touch(int argc, VALUE *argv, VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    context_t *ctx;
    VALUE args, rv, proc, exc;
    size_t nn;
    libcouchbase_error_t err;
    struct key_traits *traits;
    long seqno;

    if (bucket->handle == NULL) {
        rb_raise(eConnectError, "closed connection");
    }
    rb_scan_args(argc, argv, "0*&", &args, &proc);
    if (!bucket->async && proc != Qnil) {
        rb_raise(rb_eArgError, "synchronous mode doesn't support callbacks");
    }
    rb_funcall(args, id_flatten_bang, 0);
    traits = xcalloc(1, sizeof(struct key_traits));
    nn = cb_args_scan_keys(RARRAY_LEN(args), args, bucket, traits);
    ctx = xcalloc(1, sizeof(context_t));
    if (ctx == NULL) {
        rb_raise(eNoMemoryError, "failed to allocate memory for context");
    }
    ctx->proc = proc;
    cb_gc_protect(bucket, ctx->proc);
    ctx->bucket = bucket;
    rv = rb_hash_new();
    ctx->rv = &rv;
    ctx->exception = Qnil;
    seqno = bucket->seqno;
    bucket->seqno += nn;
    err = libcouchbase_mtouch(bucket->handle, (const void *)ctx,
            traits->nkeys, (const void * const *)traits->keys,
            traits->lens, traits->ttls);
    xfree(traits->keys);
    xfree(traits->lens);
    xfree(traits);
    exc = cb_check_error(err, "failed to schedule touch request", Qnil);
    if (exc != Qnil) {
        xfree(ctx);
        rb_exc_raise(exc);
    }
    if (bucket->async) {
        return Qnil;
    } else {
        if (bucket->seqno - seqno > 0) {
            /* we have some operations pending */
            bucket->io->run_event_loop(bucket->io);
        }
        exc = ctx->exception;
        xfree(ctx);
        if (exc != Qnil) {
            cb_gc_unprotect(bucket, exc);
            rb_exc_raise(exc);
        }
        if (bucket->exception != Qnil) {
            rb_exc_raise(bucket->exception);
        }
        if (nn > 1) {
            return rv;  /* return as a hash {key => true, ...} */
        } else {
            VALUE vv = Qnil;
            rb_hash_foreach(rv, cb_first_value_i, (VALUE)&vv);
            return vv;
        }
    }
}

/*
 * Deletes all values from a server
 *
 * @overload flush
 *   @yieldparam [Result] ret the object with +error+, +node+ and +operation+
 *     attributes.
 *
 *   @return [Boolean] +true+ on success
 *
 *   @raise [Couchbase::Error::Connect] if connection closed (see {Bucket#reconnect})
 *   @raise [ArgumentError] when passing the block in synchronous mode
 *
 *   @example Simple flush the bucket
 *     c.flush    #=> true
 *
 *   @example Asynchronous flush
 *     c.run do
 *       c.flush do |ret|
 *         ret.operation   #=> :flush
 *         ret.success?    #=> true
 *         ret.node        #=> "localhost:11211"
 *       end
 *     end
 */
    static VALUE
cb_bucket_flush(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    context_t *ctx;
    VALUE rv, exc;
    libcouchbase_error_t err;
    long seqno;

    if (bucket->handle == NULL) {
        rb_raise(eConnectError, "closed connection");
    }
    if (!bucket->async && rb_block_given_p()) {
        rb_raise(rb_eArgError, "synchronous mode doesn't support callbacks");
    }
    ctx = xcalloc(1, sizeof(context_t));
    if (ctx == NULL) {
        rb_raise(eNoMemoryError, "failed to allocate memory for context");
    }
    rv = Qtrue;	/* optimistic by default */
    ctx->rv = &rv;
    ctx->bucket = bucket;
    ctx->exception = Qnil;
    if (rb_block_given_p()) {
        ctx->proc = rb_block_proc();
    } else {
        ctx->proc = Qnil;
    }
    cb_gc_protect(bucket, ctx->proc);
    seqno = bucket->seqno;
    bucket->seqno++;
    err = libcouchbase_flush(bucket->handle, (const void *)ctx);
    exc = cb_check_error(err, "failed to schedule flush request", Qnil);
    if (exc != Qnil) {
        xfree(ctx);
        rb_exc_raise(exc);
    }
    if (bucket->async) {
        return Qnil;
    } else {
        if (bucket->seqno - seqno > 0) {
            /* we have some operations pending */
            bucket->io->run_event_loop(bucket->io);
        }
        exc = ctx->exception;
        xfree(ctx);
        if (exc != Qnil) {
            cb_gc_unprotect(bucket, exc);
            rb_exc_raise(exc);
        }
        return rv;
    }
}

/*
 * Returns versions of the server for each node in the cluster
 *
 * @overload version
 *   @yieldparam [Result] ret the object with +error+, +node+, +operation+
 *     and +value+ attributes.
 *
 *   @return [Hash] node-version pairs
 *
 *   @raise [Couchbase::Error::Connect] if connection closed (see {Bucket#reconnect})
 *   @raise [ArgumentError] when passing the block in synchronous mode
 *
 *   @example Synchronous version request
 *     c.version            #=> will render version
 *
 *   @example Asynchronous version request
 *     c.run do
 *       c.version do |ret|
 *         ret.operation    #=> :version
 *         ret.success?     #=> true
 *         ret.node         #=> "localhost:11211"
 *         ret.value        #=> will render version
 *       end
 *     end
 */
    static VALUE
cb_bucket_version(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    context_t *ctx;
    VALUE rv, exc;
    libcouchbase_error_t err;
    long seqno;

    if (bucket->handle == NULL) {
        rb_raise(eConnectError, "closed connection");
    }
    if (!bucket->async && rb_block_given_p()) {
        rb_raise(rb_eArgError, "synchronous mode doesn't support callbacks");
    }
    ctx = xcalloc(1, sizeof(context_t));
    if (ctx == NULL) {
        rb_raise(eNoMemoryError, "failed to allocate memory for context");
    }
    rv = rb_hash_new();
    ctx->rv = &rv;
    ctx->bucket = bucket;
    ctx->exception = Qnil;
    if (rb_block_given_p()) {
        ctx->proc = rb_block_proc();
    } else {
        ctx->proc = Qnil;
    }
    cb_gc_protect(bucket, ctx->proc);
    seqno = bucket->seqno;
    bucket->seqno++;
    err = libcouchbase_server_versions(bucket->handle, (const void *)ctx);
    exc = cb_check_error(err, "failed to schedule version request", Qnil);
    if (exc != Qnil) {
        xfree(ctx);
        rb_exc_raise(exc);
    }
    if (bucket->async) {
        return Qnil;
    } else {
        if (bucket->seqno - seqno > 0) {
            /* we have some operations pending */
            bucket->io->run_event_loop(bucket->io);
        }
        exc = ctx->exception;
        xfree(ctx);
        if (exc != Qnil) {
            cb_gc_unprotect(bucket, exc);
            rb_exc_raise(exc);
        }
        return rv;
    }
}

/*
 * Request server statistics.
 *
 * Fetches stats from each node in cluster. Without a key specified the
 * server will respond with a "default" set of statistical information. In
 * asynchronous mode each statistic is returned in separate call where the
 * Result object yielded (+#key+ contains the name of the statistical item
 * and the +#value+ contains the value, the +#node+ will indicate the server
 * address). In synchronous mode it returns the hash of stats keys and
 * node-value pairs as a value.
 *
 * @overload stats(arg = nil)
 *   @param [String] arg argument to STATS query
 *   @yieldparam [Result] ret the object with +node+, +key+ and +value+
 *     attributes.
 *
 *   @example Found how many items in the bucket
 *     total = 0
 *     c.stats["total_items"].each do |key, value|
 *       total += value.to_i
 *     end
 *
 *   @example Found total items number asynchronously
 *     total = 0
 *     c.run do
 *       c.stats do |ret|
 *         if ret.key == "total_items"
 *           total += ret.value.to_i
 *         end
 *       end
 *     end
 *
 *   @example Get memory stats (works on couchbase buckets)
 *     c.stats(:memory)   #=> {"mem_used"=>{...}, ...}
 *
 *   @return [Hash] where keys are stat keys, values are host-value pairs
 *
 *   @raise [Couchbase::Error::Connect] if connection closed (see {Bucket#reconnect})
 *   @raise [ArgumentError] when passing the block in synchronous mode
 */
    static VALUE
cb_bucket_stats(int argc, VALUE *argv, VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);
    context_t *ctx;
    VALUE rv, exc, arg, proc;
    char *key;
    size_t nkey;
    libcouchbase_error_t err;
    long seqno;

    if (bucket->handle == NULL) {
        rb_raise(eConnectError, "closed connection");
    }
    rb_scan_args(argc, argv, "01&", &arg, &proc);
    if (!bucket->async && proc != Qnil) {
        rb_raise(rb_eArgError, "synchronous mode doesn't support callbacks");
    }

    ctx = xcalloc(1, sizeof(context_t));
    if (ctx == NULL) {
        rb_raise(eNoMemoryError, "failed to allocate memory for context");
    }
    rv = rb_hash_new();
    ctx->rv = &rv;
    ctx->bucket = bucket;
    ctx->proc = proc;
    cb_gc_protect(bucket, ctx->proc);
    ctx->exception = Qnil;
    if (arg != Qnil) {
        arg = unify_key(arg);
        key = RSTRING_PTR(arg);
        nkey = RSTRING_LEN(arg);
    } else {
        key = NULL;
        nkey = 0;
    }
    seqno = bucket->seqno;
    bucket->seqno++;
    err = libcouchbase_server_stats(bucket->handle, (const void *)ctx,
            key, nkey);
    exc = cb_check_error(err, "failed to schedule stat request", Qnil);
    if (exc != Qnil) {
        xfree(ctx);
        rb_exc_raise(exc);
    }
    if (bucket->async) {
        return Qnil;
    } else {
        if (bucket->seqno - seqno > 0) {
            /* we have some operations pending */
            bucket->io->run_event_loop(bucket->io);
        }
        exc = ctx->exception;
        xfree(ctx);
        if (exc != Qnil) {
            cb_gc_unprotect(bucket, exc);
            rb_exc_raise(exc);
        }
        if (bucket->exception != Qnil) {
            rb_exc_raise(bucket->exception);
        }
        return rv;
    }

    return Qnil;
}

    static VALUE
do_run(VALUE *args)
{
    VALUE self = args[0], proc = args[1], exc;
    bucket_t *bucket = DATA_PTR(self);
    time_t tm;
    uint32_t old_tmo, new_tmo, diff;

    if (bucket->handle == NULL) {
        rb_raise(eConnectError, "closed connection");
    }
    if (bucket->async) {
        rb_raise(eInvalidError, "nested #run");
    }
    bucket->seqno = 0;
    bucket->async = 1;

    tm = time(NULL);
    cb_proc_call(proc, 1, self);
    if (bucket->seqno > 0) {
        old_tmo = libcouchbase_get_timeout(bucket->handle);
        diff = (uint32_t)(time(NULL) - tm + 1);
        diff *= 1000000;
        new_tmo = bucket->timeout += diff;
        libcouchbase_set_timeout(bucket->handle, bucket->timeout);
        bucket->io->run_event_loop(bucket->io);
        /* restore timeout if it wasn't changed */
        if (bucket->timeout == new_tmo) {
            libcouchbase_set_timeout(bucket->handle, old_tmo);
        }
        if (bucket->exception != Qnil) {
            exc = bucket->exception;
            bucket->exception = Qnil;
            rb_exc_raise(exc);
        }
    }
    return Qnil;
}

    static VALUE
ensure_run(VALUE *args)
{
    VALUE self = args[0];
    bucket_t *bucket = DATA_PTR(self);

    bucket->async = 0;
    return Qnil;
}

/*
 * Run the event loop.
 *
 * @yieldparam [Bucket] bucket the bucket instance
 *
 * @example Use block to run the loop
 *   c = Couchbase.new
 *   c.run do
 *     c.get("foo") {|ret| puts ret.value}
 *   end
 *
 * @example Use lambda to run the loop
 *   c = Couchbase.new
 *   operations = lambda do |c|
 *     c.get("foo") {|ret| puts ret.value}
 *   end
 *   c.run(&operations)
 *
 * @return [nil]
 *
 * @raise [Couchbase::Error::Connect] if connection closed (see {Bucket#reconnect})
 */
    static VALUE
cb_bucket_run(VALUE self)
{
    VALUE args[2];

    rb_need_block();
    args[0] = self;
    args[1] = rb_block_proc();
    rb_ensure(do_run, (VALUE)args, ensure_run, (VALUE)args);
    return Qnil;
}

/*
 * Unconditionally store the object in the Couchbase
 *
 * @overload set(key, value, options = {})
 *
 *   @param key [String, Symbol] Key used to reference the value.
 *   @param value [Object] Value to be stored
 *   @param options [Hash] Options for operation.
 *   @option options [Fixnum] :ttl (self.default_ttl) Expiry time for key.
 *     Values larger than 30*24*60*60 seconds (30 days) are interpreted as
 *     absolute times (from the epoch).
 *   @option options [Fixnum] :flags (self.default_flags) Flags for storage
 *     options. Flags are ignored by the server but preserved for use by the
 *     client. For more info see {Bucket#default_flags}.
 *   @option options [Symbol] :format (self.default_format) The
 *     representation for storing the value in the bucket. For more info see
 *     {Bucket#default_format}.
 *   @option options [Fixnum] :cas The CAS value for an object. This value
 *     created on the server and is guaranteed to be unique for each value of
 *     a given key. This value is used to provide simple optimistic
 *     concurrency control when multiple clients or threads try to update an
 *     item simultaneously.
 *
 *   @yieldparam ret [Result] the result of operation in asynchronous mode
 *     (valid attributes: +error+, +operation+, +key+).
 *
 *   @return [Fixnum] The CAS value of the object.
 *
 *   @raise [Couchbase::Error::Connect] if connection closed (see {Bucket#reconnect}).
 *   @raise [Couchbase::Error::KeyExists] if the key already exists on the
 *     server.
 *   @raise [Couchbase::Error::ValueFormat] if the value cannot be serialized
 *     with chosen encoder, e.g. if you try to store the Hash in +:plain+
 *     mode.
 *   @raise [ArgumentError] when passing the block in synchronous mode
 *
 *   @example Store the key which will be expired in 2 seconds using relative TTL.
 *     c.set("foo", "bar", :ttl => 2)
 *
 *   @example Store the key which will be expired in 2 seconds using absolute TTL.
 *     c.set("foo", "bar", :ttl => Time.now.to_i + 2)
 *
 *   @example Force JSON document format for value
 *     c.set("foo", {"bar" => "baz}, :format => :document)
 *
 *   @example Use hash-like syntax to store the value
 *     c.set["foo"] = {"bar" => "baz}
 *
 *   @example Use extended hash-like syntax
 *     c["foo", {:flags => 0x1000, :format => :plain}] = "bar"
 *     c["foo", :flags => 0x1000] = "bar"  # for ruby 1.9.x only
 *
 *   @example Set application specific flags (note that it will be OR-ed with format flags)
 *     c.set("foo", "bar", :flags => 0x1000)
 *
 *   @example Perform optimistic locking by specifying last known CAS version
 *     c.set("foo", "bar", :cas => 8835713818674332672)
 *
 *   @example Perform asynchronous call
 *     c.run do
 *       c.set("foo", "bar") do |ret|
 *         ret.operation   #=> :set
 *         ret.success?    #=> true
 *         ret.key         #=> "foo"
 *         ret.cas
 *       end
 *     end
 */
    static VALUE
cb_bucket_set(int argc, VALUE *argv, VALUE self)
{
    return cb_bucket_store(LIBCOUCHBASE_SET, argc, argv, self);
}

/*
 * Add the item to the database, but fail if the object exists already
 *
 * @overload add(key, value, options = {})
 *   @param key [String, Symbol] Key used to reference the value.
 *   @param value [Object] Value to be stored
 *   @param options [Hash] Options for operation.
 *   @option options [Fixnum] :ttl (self.default_ttl) Expiry time for key.
 *     Values larger than 30*24*60*60 seconds (30 days) are interpreted as
 *     absolute times (from the epoch).
 *   @option options [Fixnum] :flags (self.default_flags) Flags for storage
 *     options. Flags are ignored by the server but preserved for use by the
 *     client. For more info see {Bucket#default_flags}.
 *   @option options [Symbol] :format (self.default_format) The
 *     representation for storing the value in the bucket. For more info see
 *     {Bucket#default_format}.
 *   @option options [Fixnum] :cas The CAS value for an object. This value
 *     created on the server and is guaranteed to be unique for each value of
 *     a given key. This value is used to provide simple optimistic
 *     concurrency control when multiple clients or threads try to update an
 *     item simultaneously.
 *
 *   @yieldparam ret [Result] the result of operation in asynchronous mode
 *     (valid attributes: +error+, +operation+, +key+).
 *
 *   @return [Fixnum] The CAS value of the object.
 *
 *   @raise [Couchbase::Error::Connect] if connection closed (see {Bucket#reconnect})
 *   @raise [Couchbase::Error::KeyExists] if the key already exists on the
 *     server
 *   @raise [Couchbase::Error::ValueFormat] if the value cannot be serialized
 *     with chosen encoder, e.g. if you try to store the Hash in +:plain+
 *     mode.
 *   @raise [ArgumentError] when passing the block in synchronous mode
 *
 *   @example Add the same key twice
 *     c.add("foo", "bar")  #=> stored successully
 *     c.add("foo", "baz")  #=> will raise Couchbase::Error::KeyExists: failed to store value (key="foo", error=0x0c)
 */
    static VALUE
cb_bucket_add(int argc, VALUE *argv, VALUE self)
{
    return cb_bucket_store(LIBCOUCHBASE_ADD, argc, argv, self);
}

/*
 * Replace the existing object in the database
 *
 * @overload replace(key, value, options = {})
 *   @param key [String, Symbol] Key used to reference the value.
 *   @param value [Object] Value to be stored
 *   @param options [Hash] Options for operation.
 *   @option options [Fixnum] :ttl (self.default_ttl) Expiry time for key.
 *     Values larger than 30*24*60*60 seconds (30 days) are interpreted as
 *     absolute times (from the epoch).
 *   @option options [Fixnum] :flags (self.default_flags) Flags for storage
 *     options. Flags are ignored by the server but preserved for use by the
 *     client. For more info see {Bucket#default_flags}.
 *   @option options [Symbol] :format (self.default_format) The
 *     representation for storing the value in the bucket. For more info see
 *     {Bucket#default_format}.
 *
 *   @return [Fixnum] The CAS value of the object.
 *
 *   @raise [Couchbase::Error::Connect] if connection closed (see {Bucket#reconnect})
 *   @raise [Couchbase::Error::NotFound] if the key doesn't exists
 *   @raise [Couchbase::Error::KeyExists] on CAS mismatch
 *   @raise [ArgumentError] when passing the block in synchronous mode
 *
 *   @example Replacing missing key
 *     c.replace("foo", "baz")  #=> will raise Couchbase::Error::NotFound: failed to store value (key="foo", error=0x0d)
 */
    static VALUE
cb_bucket_replace(int argc, VALUE *argv, VALUE self)
{
    return cb_bucket_store(LIBCOUCHBASE_REPLACE, argc, argv, self);
}

/*
 * Append this object to the existing object
 *
 * @note This operation is kind of data-aware from server point of view.
 *   This mean that the server treats value as binary stream and just
 *   perform concatenation, therefore it won't work with +:marshal+ and
 *   +:document+ formats, because of lack of knowledge how to merge values
 *   in these formats. See Bucket#cas for workaround.
 *
 * @overload append(key, value, options = {})
 *   @param key [String, Symbol] Key used to reference the value.
 *   @param value [Object] Value to be stored
 *   @param options [Hash] Options for operation.
 *   @option options [Fixnum] :cas The CAS value for an object. This value
 *     created on the server and is guaranteed to be unique for each value of
 *     a given key. This value is used to provide simple optimistic
 *     concurrency control when multiple clients or threads try to update an
 *     item simultaneously.
 *   @option options [Symbol] :format (self.default_format) The
 *     representation for storing the value in the bucket. For more info see
 *     {Bucket#default_format}.
 *
 *   @return [Fixnum] The CAS value of the object.
 *
 *   @raise [Couchbase::Error::Connect] if connection closed (see {Bucket#reconnect})
 *   @raise [Couchbase::Error::KeyExists] on CAS mismatch
 *   @raise [Couchbase::Error::NotStored] if the key doesn't exist
 *   @raise [ArgumentError] when passing the block in synchronous mode
 *
 *   @example Simple append
 *     c.set("foo", "aaa")
 *     c.append("foo", "bbb")
 *     c.get("foo")           #=> "aaabbb"
 *
 *   @example Implementing sets using append
 *     def set_add(key, *values)
 *       encoded = values.flatten.map{|v| "+#{v} "}.join
 *       append(key, encoded)
 *     end
 *
 *     def set_remove(key, *values)
 *       encoded = values.flatten.map{|v| "-#{v} "}.join
 *       append(key, encoded)
 *     end
 *
 *     def set_get(key)
 *       encoded = get(key)
 *       ret = Set.new
 *       encoded.split(' ').each do |v|
 *         op, val = v[0], v[1..-1]
 *         case op
 *         when "-"
 *           ret.delete(val)
 *         when "+"
 *           ret.add(val)
 *         end
 *       end
 *       ret
 *     end
 *
 *   @example Using optimistic locking. The operation will fail on CAS mismatch
 *     ver = c.set("foo", "aaa")
 *     c.append("foo", "bbb", :cas => ver)
 */
    static VALUE
cb_bucket_append(int argc, VALUE *argv, VALUE self)
{
    return cb_bucket_store(LIBCOUCHBASE_APPEND, argc, argv, self);
}

/*
 * Prepend this object to the existing object
 *
 * @note This operation is kind of data-aware from server point of view.
 *   This mean that the server treats value as binary stream and just
 *   perform concatenation, therefore it won't work with +:marshal+ and
 *   +:document+ formats, because of lack of knowledge how to merge values
 *   in these formats. See Bucket#cas for workaround.
 *
 * @overload prepend(key, value, options = {})
 *   @param key [String, Symbol] Key used to reference the value.
 *   @param value [Object] Value to be stored
 *   @param options [Hash] Options for operation.
 *   @option options [Fixnum] :cas The CAS value for an object. This value
 *     created on the server and is guaranteed to be unique for each value of
 *     a given key. This value is used to provide simple optimistic
 *     concurrency control when multiple clients or threads try to update an
 *     item simultaneously.
 *   @option options [Symbol] :format (self.default_format) The
 *     representation for storing the value in the bucket. For more info see
 *     {Bucket#default_format}.
 *
 *   @raise [Couchbase::Error::Connect] if connection closed (see {Bucket#reconnect})
 *   @raise [Couchbase::Error::KeyExists] on CAS mismatch
 *   @raise [Couchbase::Error::NotStored] if the key doesn't exist
 *   @raise [ArgumentError] when passing the block in synchronous mode
 *
 *   @example Simple prepend example
 *     c.set("foo", "aaa")
 *     c.prepend("foo", "bbb")
 *     c.get("foo")           #=> "bbbaaa"
 *
 *   @example Using explicit format option
 *     c.default_format       #=> :document
 *     c.set("foo", {"y" => "z"})
 *     c.prepend("foo", '[', :format => :plain)
 *     c.append("foo", ', {"z": "y"}]', :format => :plain)
 *     c.get("foo")           #=> [{"y"=>"z"}, {"z"=>"y"}]
 *
 *   @example Using optimistic locking. The operation will fail on CAS mismatch
 *     ver = c.set("foo", "aaa")
 *     c.prepend("foo", "bbb", :cas => ver)
 */
    static VALUE
cb_bucket_prepend(int argc, VALUE *argv, VALUE self)
{
    return cb_bucket_store(LIBCOUCHBASE_PREPEND, argc, argv, self);
}

    static VALUE
cb_bucket_aset(int argc, VALUE *argv, VALUE self)
{
    VALUE temp;

    if (argc == 3) {
        /* swap opts and value, because value goes last for []= */
        temp = argv[2];
        argv[2] = argv[1];
        argv[1] = temp;
    }
    return cb_bucket_set(argc, argv, self);
}

/*
 * Close the connection to the cluster
 *
 * @return [true]
 *
 * @raise [Couchbase::Error::Connect] if connection closed (see {Bucket#reconnect})
 */
    static VALUE
cb_bucket_disconnect(VALUE self)
{
    bucket_t *bucket = DATA_PTR(self);

    if (bucket->handle) {
        libcouchbase_destroy(bucket->handle);
        bucket->handle = NULL;
        bucket->io = NULL;
        return Qtrue;
    } else {
        rb_raise(eConnectError, "closed connection");
    }
}

/*
 * Check if result of operation was successful.
 *
 * @return [Boolean] +false+ if there is an +error+ object attached,
 *   +false+ otherwise.
 */
    static VALUE
cb_result_success_p(VALUE self)
{
    return RTEST(rb_ivar_get(self, id_iv_error)) ? Qfalse : Qtrue;
}

/*
 * Returns a string containing a human-readable representation of the Result.
 *
 * @return [String]
 */
    static VALUE
cb_result_inspect(VALUE self)
{
    VALUE str, attr, error;
    char buf[100];

    str = rb_str_buf_new2("#<");
    rb_str_buf_cat2(str, rb_obj_classname(self));
    snprintf(buf, 100, ":%p", (void *)self);
    rb_str_buf_cat2(str, buf);

    attr = rb_ivar_get(self, id_iv_error);
    if (RTEST(attr)) {
        error = rb_ivar_get(attr, id_iv_error);
    } else {
        error = INT2FIX(0);
    }
    rb_str_buf_cat2(str, " error=0x");
    rb_str_append(str, rb_funcall(error, id_to_s, 1, INT2FIX(16)));

    attr = rb_ivar_get(self, id_iv_key);
    if (RTEST(attr)) {
        rb_str_buf_cat2(str, " key=");
        rb_str_append(str, rb_inspect(attr));
    }

    attr = rb_ivar_get(self, id_iv_cas);
    if (RTEST(attr)) {
        rb_str_buf_cat2(str, " cas=");
        rb_str_append(str, rb_inspect(attr));
    }

    attr = rb_ivar_get(self, id_iv_flags);
    if (RTEST(attr)) {
        rb_str_buf_cat2(str, " flags=0x");
        rb_str_append(str, rb_funcall(attr, id_to_s, 1, INT2FIX(16)));
    }

    attr = rb_ivar_get(self, id_iv_node);
    if (RTEST(attr)) {
        rb_str_buf_cat2(str, " node=");
        rb_str_append(str, rb_inspect(attr));
    }
    rb_str_buf_cat2(str, ">");

    return str;
}

/* Ruby Extension initializer */
    void
Init_couchbase_ext(void)
{
    mJSON = rb_const_get(rb_cObject, rb_intern("JSON"));
    mURI = rb_const_get(rb_cObject, rb_intern("URI"));
    mMarshal = rb_const_get(rb_cObject, rb_intern("Marshal"));
    mCouchbase = rb_define_module("Couchbase");

    mError = rb_define_module_under(mCouchbase, "Error");
    /* Document-class: Couchbase::Error::Base
     * The base error class */
    eBaseError = rb_define_class_under(mError, "Base", rb_eRuntimeError);
    /* Document-class: Couchbase::Error::Auth
     * Authentication error */
    eAuthError = rb_define_class_under(mError, "Auth", eBaseError);
    /* Document-class: Couchbase::Error::BucketNotFound
     * The given bucket not found in the cluster */
    eBucketNotFoundError = rb_define_class_under(mError, "BucketNotFound", eBaseError);
    /* Document-class: Couchbase::Error::Busy
     * The cluster is too busy now. Try again later */
    eBusyError = rb_define_class_under(mError, "Busy", eBaseError);
    /* Document-class: Couchbase::Error::DeltaBadval
     * The given value is not a number */
    eDeltaBadvalError = rb_define_class_under(mError, "DeltaBadval", eBaseError);
    /* Document-class: Couchbase::Error::Internal
     * Internal error */
    eInternalError = rb_define_class_under(mError, "Internal", eBaseError);
    /* Document-class: Couchbase::Error::Invalid
     * Invalid arguments */
    eInvalidError = rb_define_class_under(mError, "Invalid", eBaseError);
    /* Document-class: Couchbase::Error::KeyExists
     * Key already exists */
    eKeyExistsError = rb_define_class_under(mError, "KeyExists", eBaseError);
    /* Document-class: Couchbase::Error::Libcouchbase
     * Generic error */
    eLibcouchbaseError = rb_define_class_under(mError, "Libcouchbase", eBaseError);
    /* Document-class: Couchbase::Error::Libevent
     * Problem using libevent */
    eLibeventError = rb_define_class_under(mError, "Libevent", eBaseError);
    /* Document-class: Couchbase::Error::Network
     * Network error */
    eNetworkError = rb_define_class_under(mError, "Network", eBaseError);
    /* Document-class: Couchbase::Error::NoMemory
     * Out of memory error */
    eNoMemoryError = rb_define_class_under(mError, "NoMemory", eBaseError);
    /* Document-class: Couchbase::Error::NotFound
     * No such key */
    eNotFoundError = rb_define_class_under(mError, "NotFound", eBaseError);
    /* Document-class: Couchbase::Error::NotMyVbucket
     * The vbucket is not located on this server */
    eNotMyVbucketError = rb_define_class_under(mError, "NotMyVbucket", eBaseError);
    /* Document-class: Couchbase::Error::NotStored
     * Not stored */
    eNotStoredError = rb_define_class_under(mError, "NotStored", eBaseError);
    /* Document-class: Couchbase::Error::NotSupported
     * Not supported */
    eNotSupportedError = rb_define_class_under(mError, "NotSupported", eBaseError);
    /* Document-class: Couchbase::Error::Range
     * Invalid range */
    eRangeError = rb_define_class_under(mError, "Range", eBaseError);
    /* Document-class: Couchbase::Error::TemporaryFail
     * Temporary failure. Try again later */
    eTmpFailError = rb_define_class_under(mError, "TemporaryFail", eBaseError);
    /* Document-class: Couchbase::Error::TooBig
     * Object too big */
    eTooBigError = rb_define_class_under(mError, "TooBig", eBaseError);
    /* Document-class: Couchbase::Error::UnknownCommand
     * Unknown command */
    eUnknownCommandError = rb_define_class_under(mError, "UnknownCommand", eBaseError);
    /* Document-class: Couchbase::Error::UnknownHost
     * Unknown host */
    eUnknownHostError = rb_define_class_under(mError, "UnknownHost", eBaseError);
    /* Document-class: Couchbase::Error::ValueFormat
     * Failed to decode or encode value */
    eValueFormatError = rb_define_class_under(mError, "ValueFormat", eBaseError);
    /* Document-class: Couchbase::Error::Protocol
     * Protocol error */
    eProtocolError = rb_define_class_under(mError, "Protocol", eBaseError);
    /* Document-class: Couchbase::Error::Timeout
     * Timeout error */
    eTimeoutError = rb_define_class_under(mError, "Timeout", eBaseError);
    /* Document-class: Couchbase::Error::Connect
     * Connect error */
    eConnectError = rb_define_class_under(mError, "Connect", eBaseError);

    /* Document-method: error
     * @return [Boolean] the error code from libcouchbase */
    rb_define_attr(eBaseError, "error", 1, 0);
    id_iv_error = rb_intern("@error");
    /* Document-method: key
     * @return [String] the key which generated error */
    rb_define_attr(eBaseError, "key", 1, 0);
    id_iv_key = rb_intern("@key");
    /* Document-method: cas
     * @return [Fixnum] the version of the key (+nil+ unless accessible) */
    rb_define_attr(eBaseError, "cas", 1, 0);
    id_iv_cas = rb_intern("@cas");
    /* Document-method: operation
     * @return [Symbol] the operation (+nil+ unless accessible) */
    rb_define_attr(eBaseError, "operation", 1, 0);
    id_iv_operation = rb_intern("@operation");

    /* Document-class: Couchbase::Result
     * The object which yielded to asynchronous callbacks */
    cResult = rb_define_class_under(mCouchbase, "Result", rb_cObject);
    rb_define_method(cResult, "inspect", cb_result_inspect, 0);
    rb_define_method(cResult, "success?", cb_result_success_p, 0);
    /* Document-method: operation
     * @return [Symbol] */
    rb_define_attr(cResult, "operation", 1, 0);
    /* Document-method: error
     * @return [Couchbase::Error::Base] */
    rb_define_attr(cResult, "error", 1, 0);
    /* Document-method: key
     * @return [String] */
    rb_define_attr(cResult, "key", 1, 0);
    id_iv_key = rb_intern("@key");
    /* Document-method: value
     * @return [String] */
    rb_define_attr(cResult, "value", 1, 0);
    id_iv_value = rb_intern("@value");
    /* Document-method: cas
     * @return [Fixnum] */
    rb_define_attr(cResult, "cas", 1, 0);
    id_iv_cas = rb_intern("@cas");
    /* Document-method: flags
     * @return [Fixnum] */
    rb_define_attr(cResult, "flags", 1, 0);
    id_iv_flags = rb_intern("@flags");
    /* Document-method: node
     * @return [String] */
    rb_define_attr(cResult, "node", 1, 0);
    id_iv_node = rb_intern("@node");

    /* Document-class: Couchbase::Bucket
     * This class in charge of all stuff connected to communication with
     * Couchbase. */
    cBucket = rb_define_class_under(mCouchbase, "Bucket", rb_cObject);

    /* 0x03: Bitmask for flag bits responsible for format */
    rb_define_const(cBucket, "FMT_MASK", INT2FIX(FMT_MASK));
    /* 0x00: Document format. The (default) format supports most of ruby
     * types which could be mapped to JSON data (hashes, arrays, strings,
     * numbers). Future version will be able to run map/reduce queries on
     * the values in the document form (hashes). */
    rb_define_const(cBucket, "FMT_DOCUMENT", INT2FIX(FMT_DOCUMENT));
    /* 0x01:  Marshal format. The format which supports transparent
     * serialization of ruby objects with standard <tt>Marshal.dump</tt> and
     * <tt>Marhal.load</tt> methods. */
    rb_define_const(cBucket, "FMT_MARSHAL", INT2FIX(FMT_MARSHAL));
    /* 0x02:  Plain format. The format which force client don't apply any
     * conversions to the value, but it should be passed as String. It
     * could be useful for building custom algorithms or formats. For
     * example implement set:
     * http://dustin.github.com/2011/02/17/memcached-set.html */
    rb_define_const(cBucket, "FMT_PLAIN", INT2FIX(FMT_PLAIN));

    rb_define_alloc_func(cBucket, cb_bucket_alloc);
    rb_define_method(cBucket, "initialize", cb_bucket_init, -1);
    rb_define_method(cBucket, "initialize_copy", cb_bucket_init_copy, 1);
    rb_define_method(cBucket, "inspect", cb_bucket_inspect, 0);

    /* Document-method: seqno
     * The number of scheduled commands */
    /* rb_define_attr(cBucket, "seqno", 1, 0); */
    rb_define_method(cBucket, "seqno", cb_bucket_seqno, 0);

    rb_define_method(cBucket, "add", cb_bucket_add, -1);
    rb_define_method(cBucket, "append", cb_bucket_append, -1);
    rb_define_method(cBucket, "prepend", cb_bucket_prepend, -1);
    rb_define_method(cBucket, "replace", cb_bucket_replace, -1);
    rb_define_method(cBucket, "set", cb_bucket_set, -1);
    rb_define_method(cBucket, "get", cb_bucket_get, -1);
    rb_define_method(cBucket, "run", cb_bucket_run, 0);
    rb_define_method(cBucket, "touch", cb_bucket_touch, -1);
    rb_define_method(cBucket, "delete", cb_bucket_delete, -1);
    rb_define_method(cBucket, "stats", cb_bucket_stats, -1);
    rb_define_method(cBucket, "flush", cb_bucket_flush, 0);
    rb_define_method(cBucket, "version", cb_bucket_version, 0);
    rb_define_method(cBucket, "incr", cb_bucket_incr, -1);
    rb_define_method(cBucket, "decr", cb_bucket_decr, -1);
    rb_define_method(cBucket, "disconnect", cb_bucket_disconnect, 0);
    rb_define_method(cBucket, "reconnect", cb_bucket_reconnect, -1);

    rb_define_alias(cBucket, "decrement", "decr");
    rb_define_alias(cBucket, "increment", "incr");

    rb_define_alias(cBucket, "[]", "get");
    rb_define_alias(cBucket, "[]=", "set");
    rb_define_method(cBucket, "[]=", cb_bucket_aset, -1);

    rb_define_method(cBucket, "connected?", cb_bucket_connected_p, 0);
    rb_define_method(cBucket, "async?", cb_bucket_async_p, 0);

    /* Document-method: quiet
     * Flag specifying behaviour for operations on missing keys
     *
     * If it is +true+, the operations will silently return +nil+ or +false+
     * instead of raising {Couchbase::Error::NotFound}.
     *
     * @example Hiding cache miss (considering "miss" key is not stored)
     *   connection.quiet = true
     *   connection.get("miss")     #=> nil
     *
     * @example Raising errors on miss (considering "miss" key is not stored)
     *   connection.quiet = false
     *   connection.get("miss")     #=> will raise Couchbase::Error::NotFound
     *
     * @return [Boolean] */
    /* rb_define_attr(cBucket, "quiet", 1, 1); */
    rb_define_method(cBucket, "quiet", cb_bucket_quiet_get, 0);
    rb_define_method(cBucket, "quiet=", cb_bucket_quiet_set, 1);
    rb_define_alias(cBucket, "quiet?", "quiet");

    /* Document-method: default_flags
     * Default flags for new values.
     *
     * The library reserves last two lower bits to store the format of the
     * value. The can be masked via FMT_MASK constant.
     *
     * @example Selecting format bits
     *   connection.default_flags & Couchbase::Bucket::FMT_MASK
     *
     * @example Set user defined bits
     *   connection.default_flags |= 0x6660
     *
     * @note Amending format bit will also change #default_format value
     *
     * @return [Fixnum] the effective flags */
    /* rb_define_attr(cBucket, "default_flags", 1, 1); */
    rb_define_method(cBucket, "default_flags", cb_bucket_default_flags_get, 0);
    rb_define_method(cBucket, "default_flags=", cb_bucket_default_flags_set, 1);

    /* Document-method: default_format
     * Default format for new values.
     *
     * It uses flags field to store the format. It accepts either the Symbol
     * (+:document+, +:marshal+, +:plain+) or Fixnum (use constants
     * FMT_DOCUMENT, FMT_MARSHAL, FMT_PLAIN) and silently ignores all
     * other value.
     *
     * Here is some notes regarding how to choose the format:
     *
     * * <tt>:document</tt> (default) format supports most of ruby types
     *   which could be mapped to JSON data (hashes, arrays, strings,
     *   numbers). Future version will be able to run map/reduce queries on
     *   the values in the document form (hashes).
     *
     * * <tt>:plain</tt> format if you no need any conversions to be applied
     *   to your data, but your data should be passed as String. It could be
     *   useful for building custom algorithms or formats. For example
     *   implement set: http://dustin.github.com/2011/02/17/memcached-set.html
     *
     * * <tt>:marshal</tt> format if you'd like to transparently serialize
     *   your ruby object with standard <tt>Marshal.dump</tt> and
     *   <tt>Marhal.load</tt> methods.
     *
     * @example Selecting plain format using symbol
     *   connection.format = :document
     *
     * @example Selecting plain format using Fixnum constant
     *   connection.format = Couchbase::Bucket::FMT_PLAIN
     *
     * @note Amending default_format will also change #default_flags value
     *
     * @return [Symbol] the effective format */
    /* rb_define_attr(cBucket, "default_format", 1, 1); */
    rb_define_method(cBucket, "default_format", cb_bucket_default_format_get, 0);
    rb_define_method(cBucket, "default_format=", cb_bucket_default_format_set, 1);

    /* Document-method: timeout
     * @return [Fixnum] The timeout for the operations. The client will
     *   raise {Couchbase::Error::Timeout} exception for all commands which
     *   weren't completed in given timeslot. */
    /* rb_define_attr(cBucket, "timeout", 1, 1); */
    rb_define_method(cBucket, "timeout", cb_bucket_timeout_get, 0);
    rb_define_method(cBucket, "timeout=", cb_bucket_timeout_set, 1);

    /* Document-method: on_error
     * Error callback for asynchronous mode.
     *
     * This callback is using to deliver exceptions in asynchronous mode.
     *
     * @yieldparam [Symbol] op The operation caused the error
     * @yieldparam [String] key The key which cause the error or +nil+
     * @yieldparam [Exception] exc The exception instance
     *
     * @example Using lambda syntax
     *   connection = Couchbase.new(:async => true)
     *   connection.on_error = lambda {|op, key, exc| ... }
     *   connection.run do |conn|
     *     conn.set("foo", "bar")
     *   end
     *
     * @example Using block syntax
     *   connection = Couchbase.new(:async => true)
     *   connection.on_error {|op, key, exc| ... }
     *   ...
     *
     * @return [Proc] the effective callback */
    /* rb_define_attr(cBucket, "on_error", 1, 1); */
    rb_define_method(cBucket, "on_error", cb_bucket_on_error_get, 0);
    rb_define_method(cBucket, "on_error=", cb_bucket_on_error_set, 1);

    /* rb_define_attr(cBucket, "url", 1, 0); */
    rb_define_method(cBucket, "url", cb_bucket_url_get, 0);
    /* rb_define_attr(cBucket, "hostname", 1, 0); */
    rb_define_method(cBucket, "hostname", cb_bucket_hostname_get, 0);
    /* rb_define_attr(cBucket, "port", 1, 0); */
    rb_define_method(cBucket, "port", cb_bucket_port_get, 0);
    /* rb_define_attr(cBucket, "authority", 1, 0); */
    rb_define_method(cBucket, "authority", cb_bucket_authority_get, 0);
    /* rb_define_attr(cBucket, "bucket", 1, 0); */
    rb_define_method(cBucket, "bucket", cb_bucket_bucket_get, 0);
    rb_define_alias(cBucket, "name", "bucket");
    /* rb_define_attr(cBucket, "pool", 1, 0); */
    rb_define_method(cBucket, "pool", cb_bucket_pool_get, 0);
    /* rb_define_attr(cBucket, "username", 1, 0); */
    rb_define_method(cBucket, "username", cb_bucket_username_get, 0);
    /* rb_define_attr(cBucket, "password", 1, 0); */
    rb_define_method(cBucket, "password", cb_bucket_password_get, 0);

    /* Define symbols */
    id_arity = rb_intern("arity");
    id_call = rb_intern("call");
    id_dump = rb_intern("dump");
    id_dup = rb_intern("dup");
    id_flatten_bang = rb_intern("flatten!");
    id_has_key_p = rb_intern("has_key?");
    id_host = rb_intern("host");
    id_load = rb_intern("load");
    id_match = rb_intern("match");
    id_parse = rb_intern("parse");
    id_password = rb_intern("password");
    id_path = rb_intern("path");
    id_port = rb_intern("port");
    id_scheme = rb_intern("scheme");
    id_to_s = rb_intern("to_s");
    id_user = rb_intern("user");

    sym_add = ID2SYM(rb_intern("add"));
    sym_append = ID2SYM(rb_intern("append"));
    sym_bucket = ID2SYM(rb_intern("bucket"));
    sym_cas = ID2SYM(rb_intern("cas"));
    sym_create = ID2SYM(rb_intern("create"));
    sym_decrement = ID2SYM(rb_intern("decrement"));
    sym_default_flags = ID2SYM(rb_intern("default_flags"));
    sym_default_format = ID2SYM(rb_intern("default_format"));
    sym_default_ttl = ID2SYM(rb_intern("default_ttl"));
    sym_delete = ID2SYM(rb_intern("delete"));
    sym_document = ID2SYM(rb_intern("document"));
    sym_extended = ID2SYM(rb_intern("extended"));
    sym_flags = ID2SYM(rb_intern("flags"));
    sym_flush = ID2SYM(rb_intern("flush"));
    sym_format = ID2SYM(rb_intern("format"));
    sym_get = ID2SYM(rb_intern("get"));
    sym_hostname = ID2SYM(rb_intern("hostname"));
    sym_increment = ID2SYM(rb_intern("increment"));
    sym_initial = ID2SYM(rb_intern("initial"));
    sym_marshal = ID2SYM(rb_intern("marshal"));
    sym_password = ID2SYM(rb_intern("password"));
    sym_plain = ID2SYM(rb_intern("plain"));
    sym_pool = ID2SYM(rb_intern("pool"));
    sym_port = ID2SYM(rb_intern("port"));
    sym_prepend = ID2SYM(rb_intern("prepend"));
    sym_quiet = ID2SYM(rb_intern("quiet"));
    sym_replace = ID2SYM(rb_intern("replace"));
    sym_set = ID2SYM(rb_intern("set"));
    sym_stats = ID2SYM(rb_intern("stats"));
    sym_timeout = ID2SYM(rb_intern("timeout"));
    sym_touch = ID2SYM(rb_intern("touch"));
    sym_ttl = ID2SYM(rb_intern("ttl"));
    sym_username = ID2SYM(rb_intern("username"));
    sym_version = ID2SYM(rb_intern("version"));
    sym_node_list = ID2SYM(rb_intern("node_list"));
}
