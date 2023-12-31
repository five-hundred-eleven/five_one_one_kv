// this is a toy dispatch to prove that the server works.
// contains code intended to be used by both server and client.

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <Python.h>

#include "pythontypes.h"
#include "util.h"
#include "connection.h"
#include "dispatch.h"
#include "ttl.h"

// CHANGE ME
#define _FOO_KV_DEBUG 1

int16_t _dispatch_errno = 0;

int32_t dispatch(foo_kv_server *server, int32_t connid, const uint8_t *buff, int32_t len, struct response_t *response) {

    #if _FOO_KV_DEBUG == 1
    char debug_buffer[256];
    log_debug("dispatch(): got request");

    if (!response) {
        log_error("dispatch(): do not have a response object!");
        return -1;
    }
    #endif

    uint16_t nstrs;
    memcpy(&nstrs, buff, sizeof(uint16_t));

    #if _FOO_KV_DEBUG == 1
    sprintf(debug_buffer, "dispatch(): nstrs=%d", nstrs);
    log_debug(debug_buffer);
    #endif

    const uint8_t *subcmds[nstrs];
    uint16_t subcmd_to_len[nstrs];
    uint16_t offset = sizeof(uint16_t);
    int32_t err = 0;

    for (int32_t ix = 0; ix < nstrs; ix++) {
        // sanity
        if (offset >= len) {
            log_error("dispatch(): got misformed request.");
            response->status = RES_ERR_CLIENT;
            return 0;
        }
        // establish str len
        uint16_t slen;
        memcpy(&slen, buff + offset, sizeof(uint16_t));
        if (slen < 0) {
            log_error("dispatch(): got request subcmd with negative len");
            response->status = RES_ERR_CLIENT;
            return 0;
        }
        subcmd_to_len[ix] = slen;

        #if _FOO_KV_DEBUG == 1
        sprintf(debug_buffer, "dispatch(): subcmd_to_len[%d]=%d", (int)ix, slen);
        log_debug(debug_buffer);
        #endif

        // establish subcmd
        offset += sizeof(uint16_t);
        subcmds[ix] = buff + offset;

        offset += slen;

        #if _FOO_KV_DEBUG == 1
        if (slen < 200) {
            sprintf(debug_buffer, "dispatch(): subcmds[%d]=%.*s", (int)ix, slen, subcmds[ix]);
            log_debug(debug_buffer);
        } else {
            log_debug("dispatch(): subcmd too long for log");
        }
        sprintf(debug_buffer, "dispatch(): offset=%d", offset);
        log_debug(debug_buffer);
        #endif

    }

    if (offset != len) {
        if (offset > len) {
            log_error("dispatch(): got misformed request: offset overshot len");
        } else {
            log_error("dispatch(): got misformed request: offset undershot len");
        }
        response->status = RES_ERR_CLIENT;
        return 0;
    }

    int32_t cmd_hash = hash_given_len(subcmds[0], subcmd_to_len[0]);
    response->status = -1;

    #if _FOO_KV_DEBUG == 1
    if (subcmd_to_len[0] < 200) {
        sprintf(debug_buffer, "dispatch(): cmd=%.*s hash=%d", subcmd_to_len[0], subcmds[0], cmd_hash);
        log_debug(debug_buffer);
    } else {
        log_debug("dispatch(): cmd too long for buffer");
    }
    #endif

    Py_INCREF(server);
    Py_INCREF(server->storage);

    switch (cmd_hash) {
        case CMD_GET:
            err = do_get(server, subcmds + 1, subcmd_to_len + 1, nstrs - 1, response);
            break;
        case CMD_PUT:
            err = do_set(server, subcmds + 1, subcmd_to_len + 1, nstrs - 1, response);
            break;
        case CMD_DEL:
            err = do_del(server, subcmds + 1, subcmd_to_len + 1, nstrs - 1, response);
            break;
        case CMD_QUEUE:
            err = do_queue(server, subcmds + 1, subcmd_to_len + 1, nstrs - 1, response);
            break;
        case CMD_PUSH:
            err = do_push(server, subcmds + 1, subcmd_to_len + 1, nstrs - 1, response);
            break;
        case CMD_POP:
            err = do_pop(server, subcmds + 1, subcmd_to_len + 1, nstrs - 1, response);
            break;
        case CMD_TTL:
            err = do_ttl(server, subcmds + 1, subcmd_to_len + 1, nstrs - 1, response);
            break;
        default:
            log_error("dispatch(): got unrecognized command");
            response->status = RES_BAD_CMD;
            break;
    }

    #if _FOO_KV_DEBUG == 1
    log_debug("dispatch(): sanity checking storage");
    Py_ssize_t ix = 0;
    PyObject *key, *value;
    while (PyDict_Next(server->storage, &ix, &key, &value) < 0) {
        if (key->ob_refcnt <= 2 || key->ob_refcnt > 1000) {
            PyObject *as_str = PyUnicode_FromFormat("%U", key);
            PyObject *as_bytes = PyUnicode_AsUTF8String(as_str);
            sprintf(debug_buffer, "dispatch(): sanity check: key %s has %ld refcnt", PyBytes_AS_STRING(as_bytes), key->ob_refcnt);
            log_debug(debug_buffer);
            Py_DECREF(as_str);
            Py_DECREF(as_bytes);
        }
        if (value->ob_refcnt <= 2 || key->ob_refcnt > 1000) {
            PyObject *as_str = PyUnicode_FromFormat("%U", value);
            PyObject *as_bytes = PyUnicode_AsUTF8String(as_str);
            sprintf(debug_buffer, "dispatch(): sanity check: value %s has %ld refcnt", PyBytes_AS_STRING(as_bytes), value->ob_refcnt);
            log_debug(debug_buffer);
            Py_DECREF(as_str);
            Py_DECREF(as_bytes);
        }
    }
    log_debug("dispatch(): sanity check complete");
    #endif

    Py_DECREF(server->storage);
    Py_INCREF(server);

    if (response->status == -1) {
        log_warning("dispatch(): response status did not get set!");
        response->status = RES_UNKNOWN;
    }

    if (PyErr_Occurred()) {
        log_warning("dispatch(): a handler raised a Python exception that was not cleared");
        PyErr_Clear();
    }

    _dispatch_errno = 0;

    return err;

}

void error_handler(struct response_t *response) {
    PyObject *py_err = PyErr_Occurred();
    if (py_err) {
        PyErr_Clear();
    }
    #if _FOO_KV_DEBUG == 1
    char debug_buffer[256];
    sprintf(debug_buffer, "error_handler: got dispatch_errno: %hd", _dispatch_errno);
    log_error(debug_buffer);
    #endif
    switch (_dispatch_errno) {
        case RES_BAD_HASH:
            log_error("error_handler(): Failed to loads(key): not hashable");
            response->status = RES_BAD_HASH;
            break;
        case RES_BAD_TYPE:
            log_error("error_handler(): Failed to loads(key): bad type");
            response->status = RES_BAD_TYPE;
            break;
        case RES_BAD_COLLECTION:
            log_error("error_handler(): Failed to loads(key): embedded collection");
            response->status = RES_BAD_COLLECTION;
            break;
        default:
            #if _FOO_KV_DEBUG == 1
            sprintf(debug_buffer, "error_handler(): Failed to loads(key): unexpected py error type: %hd", _dispatch_errno);
            log_error(debug_buffer);
            #else
            log_error("error_handler(): Failed to loads(key): unexpected py error type");
            #endif
            response->status = RES_UNKNOWN;
    }
}

int32_t do_get(foo_kv_server *server, const uint8_t **args, const uint16_t *arg_to_len, int32_t nargs, struct response_t *response) {

    #if _FOO_KV_DEBUG == 1
    log_debug("do_get(): got request");
    #endif

    if (nargs != 1) {
        response->status = RES_BAD_ARGS;
        return 0;
    }

    PyObject *loaded_key = _loads_hashable((char *)args[0], arg_to_len[0]);
    if (!loaded_key) {
        error_handler(response);
        return 0;
    }

    // this returns a BORROWED REFERENCE, do not decref
    PyObject *py_val = PyDict_GetItem(server->storage, loaded_key);
    Py_DECREF(loaded_key);

    if (!py_val) {
        #if _FOO_KV_DEBUG == 1
        log_debug("do_get(): Failed to lookup key in storage, perhaps this is expected.");
        #endif
        PyErr_Clear();
        response->status = RES_BAD_KEY;
        return 0;
    }

    PyObject *py_res = dumps_as_pyobject(py_val);
    if (!py_res) {
        error_handler(response);
        return 0;
    }

    response->status = RES_OK;
    response->payload = py_res;

    return 0;

}

int32_t do_set(foo_kv_server *server, const uint8_t **args, const uint16_t *arg_to_len, int32_t nargs, struct response_t *response) {

    #if _FOO_KV_DEBUG == 1
    log_debug("do_set(): got request");
    #endif

    if (nargs < 2 || nargs > 3) {
        response->status = RES_BAD_ARGS;
        return 0;
    }

    PyObject *loaded_key = _loads_hashable((char *)args[0], arg_to_len[0]);
    if (!loaded_key) {
        error_handler(response);
        return 0;
    }

    #if _FOO_KV_DEBUG == 1
    log_debug("do_set(): loaded key");
    #endif

    PyObject *loaded_val = loads((char *)args[1], arg_to_len[1]);
    if (!loaded_val) {
        error_handler(response);
        return 0;
    }

    #if _FOO_KV_DEBUG == 1
    log_debug("do_set(): loaded val");
    #endif

    if (threadsafe_sem_wait(server->storage_lock)) {
        log_error("do_set(): encountered error trying to acquire storage lock");
        response->status = RES_ERR_SERVER;
        return 0;
    }

    int32_t res = PyDict_SetItem(server->storage, loaded_key, loaded_val);

    Py_DECREF(loaded_key);
    Py_DECREF(loaded_val);

    if (sem_post(server->storage_lock)) {
        log_error("do_set(): failed to release lock");
        response->status = RES_ERR_SERVER;
        return 0;
    }

    if (nargs == 3) {
        PyObject *loaded_ttl = _loads_foo_datetime((char *)args[2], arg_to_len[2]);
        if (!loaded_ttl) {
            error_handler(response);
            return 0;
        }
        #if _FOO_KV_DEBUG == 1
        log_debug("do_set(): loaded ttl");
        #endif
        if (foo_kv_ttl_heap_put_dt(server->storage_ttl_heap, loaded_key, loaded_ttl)) {
            log_error("do_set(): unable to set ttl on item, exiting!");
            response->status = RES_ERR_SERVER;
            return 0;
        }
    } else {
        if (foo_kv_ttl_heap_invalidate(server->storage_ttl_heap, loaded_key)) {
            log_error("do_set(): unable to invalidate previous ttl, exiting");
            response->status = RES_ERR_SERVER;
            return 0;
        }
    }

    if (res) {
        log_error("do_set(): got error setting item in storage: perhaps this is expected");
        response->status = RES_ERR_SERVER;
        return 0;
    }

    response->status = RES_OK;
    return 0;

}

int32_t do_del(foo_kv_server *server, const uint8_t **args, const uint16_t *arg_to_len, int32_t nargs, struct response_t *response) {

    #if _FOO_KV_DEBUG == 1
    log_debug("do_del(): got request");
    #endif

    if (nargs != 1) {
        response->status = RES_BAD_ARGS;
        return 0;
    }

    int32_t res;

    PyObject *loaded_key = _loads_hashable((char *)args[0], arg_to_len[0]);
    if (!loaded_key) {
        error_handler(response);
        return 0;
    }

    #if _FOO_KV_DEBUG == 1
    log_debug("do_del(): got past py_key");

    if (server->storage == NULL) {
        log_error("do_del(): server storage has become NULL!!!");
        response->status = RES_ERR_SERVER;
        return -1;
    }
    #endif

    if (threadsafe_sem_wait(server->storage_lock)) {
        log_error("do_del(): sem_wait() failed");
        response->status = RES_ERR_SERVER;
        return -1;
    }

    #if _FOO_KV_DEBUG == 1
    log_debug("do_del(): got past sem_wait");
    #endif

    /*
    res = PyDict_Contains(server->storage, loaded_key);
    if (res != 1) {
        if (res == 0) {
            log_debug("do_del(): storage does not contain key, perhaps this is expected");
            response->status = RES_BAD_KEY;
        } else {
            log_error("do_del(): server storage contains returned error!");
            response->status = RES_ERR_SERVER;
        }
        if (sem_post(server->storage_lock)) {
            log_error("do_del(): sem_post() failed");
            response->status = RES_ERR_SERVER;
            return -1;
        }
        return 0;
    }  // storage contains key if we got here

    #if _FOO_KV_DEBUG == 1
    log_debug("do_del(): storage contains key");
    #endif
    */

    // PyDict_DelItem segfaults randomly
    res = _pyobject_safe_delitem(server->storage, loaded_key);
    //res = PyDict_DelItem(server->storage, loaded_key);
    Py_DECREF(loaded_key);
    if (res < 0) {
        log_error("do_del(): py operation resulted in error");
        if (PyErr_Occurred()) {
            PyErr_Clear();
        }
        response->status = RES_ERR_SERVER;
        return -1;
    }

    #if _FOO_KV_DEBUG == 1
    log_debug("do_del(): got res");
    #endif

    if (sem_post(server->storage_lock)) {
        log_error("do_del(): sem_post() failed");
        response->status = RES_ERR_SERVER;
        return -1;
    }

    #if _FOO_KV_DEBUG == 1
    log_debug("do_del(): got past release lock");
    #endif

    if (res == 0) {
        #if _FOO_KV_DEBUG == 1
        log_debug("do_del(): key was not in storage: perhaps this is expected");
        #endif
        response->status = RES_BAD_KEY;
        return 0;
    }

    #if _FOO_KV_DEBUG == 1
    log_debug("do_del(): sending successful response");
    #endif

    response->status = RES_OK;
    return 0;

}

int32_t do_queue(foo_kv_server *server, const uint8_t **args, const uint16_t *arg_to_len, int32_t nargs, struct response_t *response) {

    #if _FOO_KV_DEBUG == 1
    log_debug("do_queue(): got request");
    #endif

    if (nargs < 1 || nargs > 2) {
        response->status = RES_BAD_ARGS;
        return 0;
    }

    PyObject *loaded_key = _loads_hashable((char *)args[0], arg_to_len[0]);
    if (!loaded_key) {
        error_handler(response);
        return 0;
    }

    #if _FOO_KV_DEBUG == 1
    log_debug("do_queue(): loaded key");
    #endif

    PyObject *deq_obj = PyObject_CallNoArgs(_deq_class);
    if (!deq_obj) {
        _dispatch_errno = RES_ERR_SERVER;
        return 0;
    }

    #if _FOO_KV_DEBUG == 1
    log_debug("do_queue(): loaded val");
    #endif

    if (threadsafe_sem_wait(server->storage_lock)) {
        log_error("do_queue(): encountered error trying to acquire storage lock");
        response->status = RES_ERR_SERVER;
        return 0;
    }

    int32_t res = PyDict_SetItem(server->storage, loaded_key, deq_obj);

    Py_DECREF(loaded_key);
    Py_DECREF(deq_obj);

    if (sem_post(server->storage_lock)) {
        log_error("do_queue(): failed to release lock");
        response->status = RES_ERR_SERVER;
        return 0;
    }

    if (nargs == 3) {
        PyObject *loaded_ttl = _loads_foo_datetime((char *)args[2], arg_to_len[2]);
        if (!loaded_ttl) {
            error_handler(response);
            return 0;
        }
        #if _FOO_KV_DEBUG == 1
        log_debug("do_queue(): loaded ttl");
        #endif
        if (foo_kv_ttl_heap_put_dt(server->storage_ttl_heap, loaded_key, loaded_ttl)) {
            log_error("do_queue(): unable to set ttl on item, exiting!");
            response->status = RES_ERR_SERVER;
            return 0;
        }
    } else {
        if (foo_kv_ttl_heap_invalidate(server->storage_ttl_heap, loaded_key)) {
            log_error("do_queue(): unable to invalidate previous ttl, exiting");
            response->status = RES_ERR_SERVER;
            return 0;
        }
    }

    if (res) {
        log_error("do_queue(): got error setting item in storage: perhaps this is expected");
        response->status = RES_ERR_SERVER;
        return 0;
    }

    response->status = RES_OK;
    return 0;

}

int32_t do_push(foo_kv_server *server, const uint8_t **args, const uint16_t *arg_to_len, int32_t nargs, struct response_t *response) {

    #if _FOO_KV_DEBUG == 1
    log_debug("do_push(): got request");
    #endif

    if (nargs != 2) {
        response->status = RES_BAD_ARGS;
        return 0;
    }

    PyObject *loaded_key = _loads_hashable((char *)args[0], arg_to_len[0]);
    if (!loaded_key) {
        error_handler(response);
        return 0;
    }

    #if _FOO_KV_DEBUG == 1
    log_debug("do_push(): loaded key");
    #endif

    PyObject *loaded_val = _loads_collectable((char *)args[1], arg_to_len[1]);
    if (!loaded_val) {
        error_handler(response);
        return 0;
    }

    #if _FOO_KV_DEBUG == 1
    log_debug("do_push(): loaded val");
    #endif

    // TODO seems like just the deq should be locked, in opposed to all of storage?
    if (threadsafe_sem_wait(server->storage_lock)) {
        log_error("do_push(): encountered error trying to acquire storage lock");
        response->status = RES_ERR_SERVER;
        return -1;
    }

    int32_t err = 0;

    PyObject *deq_obj = PyDict_GetItem(server->storage, loaded_key);
    Py_DECREF(loaded_key);
    if (!deq_obj) {
        if (PyErr_Occurred()) {
            PyErr_Clear();
        }
        response->status = RES_BAD_KEY;
        goto DO_PUSH_END;
    }

    Py_INCREF(_deq_class);
    int32_t is_valid = PyObject_IsInstance(deq_obj, _deq_class);
    Py_DECREF(_deq_class);
    if (is_valid < 0) {
        if (PyErr_Occurred()) {
            PyErr_Clear();
        }
        log_error("do_push(): could not determine if item is a deque");
        response->status = RES_ERR_SERVER;
        goto DO_PUSH_END;
    }
    if (is_valid == 0) {
        log_error("do_push(): item at key is not a deq");
        response->status = RES_BAD_OP;
        goto DO_PUSH_END;
        return 0;
    }

    PyObject *push_result = PyObject_CallMethodObjArgs(deq_obj, _append_str, loaded_val, NULL);
    Py_DECREF(loaded_val);

    if (!push_result) {
        if (PyErr_Occurred()) {
            PyErr_Clear();
        }
        log_error("do_push(): append call raised an error");
        response->status = RES_ERR_SERVER;
        goto DO_PUSH_END;
    }
    Py_DECREF(push_result);

    response->status = RES_OK;

DO_PUSH_END:
    if (sem_post(server->storage_lock)) {
        log_error("do_push(): failed to release lock");
        response->status = RES_ERR_SERVER;
        return -1;
    }

    return err;

}

int32_t do_pop(foo_kv_server *server, const uint8_t **args, const uint16_t *arg_to_len, int32_t nargs, struct response_t *response) {

    #if _FOO_KV_DEBUG == 1
    log_debug("do_pop(): got request");
    #endif

    if (nargs != 1) {
        response->status = RES_BAD_ARGS;
        return 0;
    }

    PyObject *loaded_key = _loads_hashable((char *)args[0], arg_to_len[0]);
    if (!loaded_key) {
        error_handler(response);
        return 0;
    }

    // TODO seems like just the deq should be locked, in opposed to all of storage?
    if (threadsafe_sem_wait(server->storage_lock)) {
        log_error("do_pop(): encountered error trying to acquire storage lock");
        response->status = RES_ERR_SERVER;
        return -1;
    }

    PyObject *deq_obj = PyDict_GetItem(server->storage, loaded_key);
    Py_DECREF(loaded_key);
    if (!deq_obj) {
        if (PyErr_Occurred()) {
            PyErr_Clear();
        }
        response->status = RES_BAD_KEY;
        goto DO_POP_END;
    }

    Py_INCREF(_deq_class);
    int32_t is_valid = PyObject_IsInstance(deq_obj, _deq_class);
    Py_DECREF(_deq_class);
    if (is_valid < 0) {
        if (PyErr_Occurred()) {
            PyErr_Clear();
        }
        log_error("do_pop(): could not determine if item is a deque");
        response->status = RES_ERR_SERVER;
        goto DO_POP_END;
    }
    if (is_valid == 0) {
        log_error("do_pop(): item at key is not a deque");
        response->status = RES_BAD_OP;
        goto DO_POP_END;
        return 0;
    }

    PyObject *pop_result = PyObject_CallMethodObjArgs(deq_obj, _popleft_str, NULL);
    if (!pop_result) {
        if (PyErr_Occurred()) {
            PyErr_Clear();
        }
        response->status = RES_BAD_IX;
        goto DO_POP_END;
    }

    PyObject *dumped_result = dumps_as_pyobject(pop_result);
    Py_DECREF(pop_result);
    if (!dumped_result) {
        log_error("do_pop(): was not able to dump item");
        error_handler(response);
        goto DO_POP_END;
    }

    response->status = RES_OK;
    response->payload = dumped_result;

DO_POP_END:
    if (sem_post(server->storage_lock)) {
        log_error("do_pop(): failed to release lock");
        response->status = RES_ERR_SERVER;
        return -1;
    }

    return 0;
}

int32_t do_ttl(foo_kv_server *server, const uint8_t **args, const uint16_t *arg_to_len, int32_t nargs, struct response_t *response) {

    #if _FOO_KV_DEBUG == 1
    log_debug("do_ttl(): got request");
    #endif

    if (nargs < 1 || nargs > 2) {
        response->status = RES_BAD_ARGS;
        return 0;
    }

    PyObject *loaded_key = _loads_hashable((char *)args[0], arg_to_len[0]);
    if (!loaded_key) {
        error_handler(response);
        return 0;
    }

    #if _FOO_KV_DEBUG == 1
    log_debug("do_ttl(): loaded key");
    #endif

    int32_t is_contained = PyDict_Contains(server->storage, loaded_key);
    if (is_contained < 0) {
        log_error("do_ttl(): could not determine if key exists");
        response->status = RES_ERR_SERVER;
        return 0;
    }
    if (is_contained == 0) {
        log_error("do_ttl(): key is not contained, cannot set ttl.");
        response->status = RES_BAD_KEY;
        return 0;
    }

    if (nargs == 2) {
        PyObject *loaded_ttl = _loads_foo_datetime((char *)args[1], arg_to_len[1]);
        if (!loaded_ttl) {
            error_handler(response);
            return 0;
        }
        #if _FOO_KV_DEBUG == 1
        log_debug("do_ttl(): loaded ttl");
        #endif
        if (foo_kv_ttl_heap_put_dt(server->storage_ttl_heap, loaded_key, loaded_ttl)) {
            log_error("do_ttl(): unable to set ttl on item, exiting!");
            response->status = RES_ERR_SERVER;
            return 0;
        }
    } else {
        if (foo_kv_ttl_heap_invalidate(server->storage_ttl_heap, loaded_key)) {
            log_error("do_ttl(): unable to invalidate previous ttl, exiting");
            response->status = RES_ERR_SERVER;
            return 0;
        }
    }

    response->status = RES_OK;
    return 0;

}




// helper methods
PyObject *dumps_as_pyobject(PyObject *x) {

    PyObject *x_type = PyObject_Type(x);
    if (!x_type) {
        return NULL;
    }
    Py_INCREF(_type_to_symbol);
    PyObject *symbol = PyDict_GetItem(_type_to_symbol, x_type);
    Py_DECREF(_type_to_symbol);
    Py_DECREF(x_type);
    if (!symbol) {
        return NULL;
    }

    char s = *PyBytes_AS_STRING(symbol);

    switch (s) {
        case INT_SYMBOL:
            return _dumps_long(x);
        case FLOAT_SYMBOL:
            return _dumps_float(x);
        case STRING_SYMBOL:
            return _dumps_unicode(x);
        case BYTES_SYMBOL:
            return PyBytes_FromFormat("%c%s", BYTES_SYMBOL, PyBytes_AS_STRING(x));
        case LIST_SYMBOL:
            return _dumps_list(x);
        case TUPLE_SYMBOL:
            return _dumps_tuple(x);
        case BOOL_SYMBOL:
            if (Py_IsTrue(x)) {
                return PyBytes_FromFormat("%c%c", s, '1');
            }
            if (Py_IsFalse(x)) {
                return PyBytes_FromFormat("%c%c", s, '0');
            }
            return NULL;
        case DATETIME_SYMBOL:
            return _dumps_datetime(x);
        default:
            _dispatch_errno = RES_BAD_TYPE;
            return NULL;
    }

    return NULL;

}

// the following is deprecated and also doesn't really work
// because the PyObject refcount is not properly handled
const char *dumps(PyObject *x) {

    PyObject *res = dumps_as_pyobject(x);
    if (!res) {
        return NULL;
    }
    return PyBytes_AS_STRING(res);

}

PyObject *_dumps_long(PyObject *x) {
    PyObject *xs = PyObject_Str(x);
    if (PyErr_Occurred()) {
        PyErr_Clear();
        return NULL;
    }
    PyObject *xb = PyUnicode_AsASCIIString(xs);
    if (PyErr_Occurred()) {
        PyErr_Clear();
        return NULL;
    }
    PyObject *result = PyBytes_FromFormat("%c%s", INT_SYMBOL, PyBytes_AS_STRING(xb));
    if (PyErr_Occurred()) {
        PyErr_Clear();
        return NULL;
    }
    Py_DECREF(xs);
    Py_DECREF(xb);
    return result;
}

PyObject *_dumps_float(PyObject *x) {
    PyObject *xs = PyObject_Str(x);
    if (PyErr_Occurred()) {
        PyErr_Clear();
        return NULL;
    }
    PyObject *xb = PyUnicode_AsASCIIString(xs);
    if (PyErr_Occurred()) {
        PyErr_Clear();
        return NULL;
    }
    PyObject *result = PyBytes_FromFormat("%c%s", FLOAT_SYMBOL, PyBytes_AS_STRING(xb));
    if (PyErr_Occurred()) {
        PyErr_Clear();
        return NULL;
    }
    Py_DECREF(xs);
    Py_DECREF(xb);
    return result;
}

PyObject *_dumps_unicode(PyObject *x) {
    PyObject *b = PyUnicode_AsUTF8String(x);
    if (!b) {
        return NULL;
    }
    PyObject *res = PyBytes_FromFormat("%c%s", STRING_SYMBOL, PyBytes_AS_STRING(b));
    Py_DECREF(b);
    return res;
}

PyObject *_dumps_list(PyObject *x) {
    uint16_t size = PyList_GET_SIZE(x);
    char symbol = LIST_SYMBOL;
    PyObject *intermediate = PyList_New(size);
    if (!intermediate) {
        return NULL;
    }
    #if _FOO_KV_DEBUG == 1
    char debug_buff[256];
    #endif
    // char for type declaration, int16 for the number of items
    int32_t buffer_len = sizeof(char) + sizeof(uint16_t);
    for (Py_ssize_t ix = 0; ix < size; ix++) {
        PyObject *dumped_item = _dumps_collectable_as_pyobject(PyList_GET_ITEM(x, ix));
        if (!dumped_item) {
            Py_DECREF(intermediate);
            return NULL;
        }
        PyList_SET_ITEM(intermediate, ix, dumped_item);
        buffer_len += sizeof(uint16_t) + PyBytes_GET_SIZE(dumped_item);
    }
    char buffer[buffer_len];
    int32_t offset = 0;
    memcpy(buffer + offset, &symbol, sizeof(char));
    offset += sizeof(char);
    memcpy(buffer + offset, &size, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    for (Py_ssize_t ix = 0; ix < size; ix++) {
        #if _FOO_KV_DEBUG == 1
        sprintf(debug_buff, "_dumps_list(): offset: %d", offset);
        log_debug(debug_buff);
        #endif
        PyObject *item = PyList_GET_ITEM(intermediate, ix);
        uint16_t slen = PyBytes_GET_SIZE(item);
        #if _FOO_KV_DEBUG == 1
        sprintf(debug_buff, "_dumps_list(): len(items[%ld])=%hu", ix, slen);
        log_debug(debug_buff);
        sprintf(debug_buff, "_dumps_list(): items[%ld]=%.*s", ix, slen, PyBytes_AS_STRING(item));
        log_debug(debug_buff);
        #endif
        memcpy(buffer + offset, &slen, sizeof(uint16_t));
        offset += sizeof(uint16_t);
        memcpy(buffer + offset, PyBytes_AS_STRING(item), slen);
        offset += slen;
    }

    if (offset != buffer_len) {
        #if _FOO_KV_DEBUG == 1
        if (offset > buffer_len) {
            log_error("_dumps_list(): offset overshot expected buffer_len");
        } else {
            log_error("_dumps_list(): offset undershot expected buffer_len");
        }
        #endif
        _dispatch_errno = RES_ERR_SERVER;
        return NULL;
    }

    Py_DECREF(intermediate);

    return PyBytes_FromStringAndSize(buffer, buffer_len);

}

PyObject *_dumps_tuple(PyObject *x) {
    uint16_t size = PyTuple_GET_SIZE(x);
    char symbol = TUPLE_SYMBOL;
    PyObject *intermediate = PyList_New(size);
    if (!intermediate) {
        return NULL;
    }
    #if _FOO_KV_DEBUG == 1
    char debug_buff[256];
    #endif
    // char for type declaration, int16 for the number of items
    int32_t buffer_len = sizeof(char) + sizeof(uint16_t);
    for (Py_ssize_t ix = 0; ix < size; ix++) {
        PyObject *dumped_item = _dumps_collectable_as_pyobject(PyTuple_GET_ITEM(x, ix));
        if (!dumped_item) {
            Py_DECREF(intermediate);
            return NULL;
        }
        PyList_SET_ITEM(intermediate, ix, dumped_item);
        buffer_len += sizeof(uint16_t) + PyBytes_GET_SIZE(dumped_item);
    }
    char buffer[buffer_len];
    int32_t offset = 0;
    memcpy(buffer + offset, &symbol, sizeof(char));
    offset += sizeof(char);
    memcpy(buffer + offset, &size, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    for (Py_ssize_t ix = 0; ix < size; ix++) {
        #if _FOO_KV_DEBUG == 1
        sprintf(debug_buff, "_dumps_tuple(): offset: %d", offset);
        log_debug(debug_buff);
        #endif
        PyObject *item = PyList_GET_ITEM(intermediate, ix);
        uint16_t slen = PyBytes_GET_SIZE(item);
        #if _FOO_KV_DEBUG == 1
        sprintf(debug_buff, "_dumps_tuple(): len(items[%ld])=%hu", ix, slen);
        log_debug(debug_buff);
        sprintf(debug_buff, "_dumps_tuple(): items[%ld]=%.*s", ix, slen, PyBytes_AS_STRING(item));
        log_debug(debug_buff);
        #endif
        memcpy(buffer + offset, &slen, sizeof(uint16_t));
        offset += sizeof(uint16_t);
        memcpy(buffer + offset, PyBytes_AS_STRING(item), slen);
        offset += slen;
    }

    if (offset != buffer_len) {
        #if _FOO_KV_DEBUG == 1
        if (offset > buffer_len) {
            log_error("_dumps_tuple(): offset overshot expected buffer_len");
        } else {
            log_error("_dumps_tuple(): offset undershot expected buffer_len");
        }
        #endif
        _dispatch_errno = RES_ERR_SERVER;
        return NULL;
    }

    Py_DECREF(intermediate);

    return PyBytes_FromStringAndSize(buffer, buffer_len);

}

PyObject *_dumps_hashable_tuple(PyObject *x) {
    uint16_t size = PyTuple_GET_SIZE(x);
    char symbol = TUPLE_SYMBOL;
    PyObject *intermediate = PyList_New(size);
    if (!intermediate) {
        return NULL;
    }
    #if _FOO_KV_DEBUG == 1
    char debug_buff[256];
    #endif
    // char for type declaration, int16 for the number of items
    int32_t buffer_len = sizeof(char) + sizeof(uint16_t);
    for (Py_ssize_t ix = 0; ix < size; ix++) {
        PyObject *dumped_item = _dumps_hashable_as_pyobject(PyTuple_GET_ITEM(x, ix));
        if (!dumped_item) {
            Py_DECREF(intermediate);
            return NULL;
        }
        PyList_SET_ITEM(intermediate, ix, dumped_item);
        buffer_len += sizeof(uint16_t) + PyBytes_GET_SIZE(dumped_item);
    }
    char buffer[buffer_len];
    int32_t offset = 0;
    memcpy(buffer + offset, &symbol, sizeof(char));
    offset += sizeof(char);
    memcpy(buffer + offset, &size, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    for (Py_ssize_t ix = 0; ix < size; ix++) {
        #if _FOO_KV_DEBUG == 1
        sprintf(debug_buff, "_dumps_hashable_tuple(): offset: %d", offset);
        log_debug(debug_buff);
        #endif
        PyObject *item = PyList_GET_ITEM(intermediate, ix);
        uint16_t slen = PyBytes_GET_SIZE(item);
        #if _FOO_KV_DEBUG == 1
        sprintf(debug_buff, "_dumps_hashable_tuple(): len(items[%ld])=%hu", ix, slen);
        log_debug(debug_buff);
        sprintf(debug_buff, "_dumps_hashable_tuple(): items[%ld]=%.*s", ix, slen, PyBytes_AS_STRING(item));
        log_debug(debug_buff);
        #endif
        memcpy(buffer + offset, &slen, sizeof(uint16_t));
        offset += sizeof(uint16_t);
        memcpy(buffer + offset, PyBytes_AS_STRING(item), slen);
        offset += slen;
    }

    if (offset != buffer_len) {
        #if _FOO_KV_DEBUG == 1
        if (offset > buffer_len) {
            log_error("_dumps_hashable_tuple(): offset overshot expected buffer_len");
        } else {
            log_error("_dumps_hashable_tuple(): offset undershot expected buffer_len");
        }
        #endif
        _dispatch_errno = RES_ERR_SERVER;
        return NULL;
    }

    Py_DECREF(intermediate);

    return PyBytes_FromStringAndSize(buffer, buffer_len);

}

PyObject *_dumps_datetime(PyObject *x) {

    PyObject *dts = PyObject_CallMethodOneArg(x, _strftime_str, _datetime_formatstring);
    if (!dts) {
        if (PyErr_Occurred()) {
            PyErr_Clear();
        }
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }

    PyObject *dts_with_symbol = PyUnicode_FromFormat("%c%U", DATETIME_SYMBOL, dts);
    Py_DECREF(dts);
    if (!dts_with_symbol) {
        if (PyErr_Occurred()) {
            PyErr_Clear();
        }
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }

    PyObject *result = PyUnicode_AsUTF8String(dts_with_symbol);
    Py_DECREF(dts_with_symbol);
    if (!result) {
        if (PyErr_Occurred()) {
            PyErr_Clear();
        }
        _dispatch_errno = RES_UNKNOWN;
        return NULL;
    }

    return result;

}

PyObject *_dumps_hashable_as_pyobject(PyObject *x) {

    PyObject *x_type = PyObject_Type(x);
    if (!x_type) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }
    PyObject *symbol = PyDict_GetItem(_type_to_symbol, x_type);
    Py_DECREF(x_type);
    if (!symbol) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }

    char s = *PyBytes_AS_STRING(symbol);

    switch (s) {
        case INT_SYMBOL:
            return _dumps_long(x);
        case FLOAT_SYMBOL:
            return _dumps_float(x);
        case STRING_SYMBOL:
            return _dumps_unicode(x);
        case BYTES_SYMBOL:
            return PyBytes_FromFormat("%c%s", BYTES_SYMBOL, PyBytes_AS_STRING(x));
        case TUPLE_SYMBOL:
            return _dumps_hashable_tuple(x);
        case LIST_SYMBOL:
        case BOOL_SYMBOL:
        case DATETIME_SYMBOL:
            _dispatch_errno = RES_BAD_HASH;
            return NULL;
        default:
            _dispatch_errno = RES_BAD_TYPE;
            return NULL;
    }

    return NULL;

}


PyObject *_dumps_collectable_as_pyobject(PyObject *x) {

    PyObject *x_type = PyObject_Type(x);
    if (!x_type) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }
    PyObject *symbol = PyDict_GetItem(_type_to_symbol, x_type);
    Py_DECREF(x_type);
    if (!symbol) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }

    char s = *PyBytes_AS_STRING(symbol);

    switch (s) {
        case INT_SYMBOL:
            return _dumps_long(x);
        case FLOAT_SYMBOL:
            return _dumps_float(x);
        case STRING_SYMBOL:
            return _dumps_unicode(x);
        case BYTES_SYMBOL:
            return PyBytes_FromFormat("%c%s", s, PyBytes_AS_STRING(x));
        case TUPLE_SYMBOL:
            return _dumps_tuple(x);
        case LIST_SYMBOL:
            _dispatch_errno = RES_BAD_COLLECTION;
            return NULL;
        case BOOL_SYMBOL:
            if (Py_IsTrue(x)) {
                return PyBytes_FromFormat("%c%c", BOOL_SYMBOL, '1');
            }
            if (Py_IsFalse(x)) {
                return PyBytes_FromFormat("%c%c", BOOL_SYMBOL, '0');
            }
            return NULL;
        case DATETIME_SYMBOL:
            return _dumps_datetime(x);
        default:
            _dispatch_errno = RES_BAD_TYPE;
            return NULL;
    }

    return NULL;

}

PyObject *_dumps_datetime_as_pyobject(PyObject *x) {
    return _dumps_datetime(x);
}


PyObject *loads_from_pyobject(PyObject *x) {

    char *res = PyBytes_AsString(x);
    if (!res) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }

    int32_t len = PyObject_Length(x);
    if (!len) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }

    return loads(res, len);

}

PyObject *loads(const char *x, int32_t len) {

    if ((uint32_t)len < sizeof(char)) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }

    switch (x[0]) {
        case INT_SYMBOL:
            return _loads_long(x + 1, len - 1);
        case FLOAT_SYMBOL:
            return _loads_float(x + 1, len - 1);
        case STRING_SYMBOL:
            return _loads_unicode(x + 1, len - 1);
        case BYTES_SYMBOL:
            return PyBytes_FromStringAndSize(x + 1, len - 1);
        case LIST_SYMBOL:
            return _loads_list(x + 1, len - 1);
        case BOOL_SYMBOL:
            return _loads_bool(x + 1, len - 1);
        case DATETIME_SYMBOL:
            return _loads_datetime(x + 1, len - 1);
        case TUPLE_SYMBOL:
            return _loads_tuple(x + 1, len - 1);
        default:
            _dispatch_errno = RES_BAD_TYPE;
            return NULL;
    }

    return NULL;

}

PyObject *_loads_long(const char *x, int32_t len) {

    PyObject *xs = PyUnicode_FromStringAndSize(x, len);
    if (!xs) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }

    PyObject *rv = PyLong_FromUnicodeObject(xs, 0);
    Py_DECREF(xs);
    if (!rv) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }

    return rv;

}

PyObject *_loads_float(const char *x, int32_t len) {
    PyObject *intermediate = PyUnicode_FromStringAndSize(x, len);
    if (!intermediate) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }
    PyObject *rv = PyFloat_FromString(intermediate);
    Py_DECREF(intermediate);
    if (!rv) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }
    return rv;
}

PyObject *_loads_unicode(const char *x, int32_t len) {
    PyObject *rv = PyUnicode_DecodeUTF8(x, len, "strict");
    if (!rv) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }
    return rv;
}

PyObject *_loads_bool(const char *x, int32_t len) {

    if (len != 1) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }
    if (x[0] == '0') {
        Py_RETURN_FALSE;
    }
    if (x[0] == '1') {
        Py_RETURN_TRUE;
    }

    _dispatch_errno = RES_BAD_TYPE;
    return NULL;

}

PyObject *_loads_list(const char *x, int32_t len) {

    if ((uint16_t)len < sizeof(uint16_t)) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }

    uint16_t nstrs;
    memcpy(&nstrs, x, sizeof(uint16_t));

    #if _FOO_KV_DEBUG == 1
    char debug_buffer[256];
    sprintf(debug_buffer, "_loads_list(): nstrs=%hu, len=%d", nstrs, len);
    log_debug(debug_buffer);
    #endif

    const char *items[nstrs];
    uint16_t item_to_len[nstrs];
    uint16_t offset = sizeof(uint16_t);

    for (int32_t ix = 0; ix < nstrs; ix++) {
        #if _FOO_KV_DEBUG == 1
        sprintf(debug_buffer, "_loads_list(): offset=%hu", offset);
        log_debug(debug_buffer);
        #endif
        // sanity
        if (offset >= len) {
            log_error("_loads_list(): got misformed request.");
            _dispatch_errno = RES_ERR_CLIENT;
            return NULL;
        }
        // establish str len
        uint16_t slen;
        memcpy(&slen, x + offset, sizeof(uint16_t));
        if (slen < sizeof(char)) {
            log_error("_loads_list(): got request item with not enough size for type indicator");
            _dispatch_errno = RES_ERR_CLIENT;
            return NULL;
        }
        item_to_len[ix] = slen;

        #if _FOO_KV_DEBUG == 1
        sprintf(debug_buffer, "_loads_list(): item_to_len[%d]=%d", (int)ix, slen);
        log_debug(debug_buffer);
        #endif

        // establish subcmd
        offset += sizeof(uint16_t);
        items[ix] = x + offset;

        offset += slen;

        #if _FOO_KV_DEBUG == 1
        if (slen < 200) {
            sprintf(debug_buffer, "_loads_list(): items[%d]=%.*s", (int)ix, slen, items[ix]);
            log_debug(debug_buffer);
        } else {
            log_debug("_loads_list(): subcmd too long for log");
        }
        #endif

    }

    if (offset != len) {
        if (offset > len) {
            log_error("_loads_list(): got malformed request: offset overshot len");
        } else {
            log_error("_loads_list(): got malformed request: offset undershot len");
        }
        _dispatch_errno = RES_ERR_CLIENT;
        return NULL;
    }

    PyObject *result = PyList_New(nstrs);
    for (Py_ssize_t ix = 0; ix < nstrs; ix++) {
        PyObject *loaded_item = _loads_collectable(items[ix], item_to_len[ix]);
        if (!loaded_item) {
            Py_DECREF(result);
            return NULL;
        }
        PyList_SET_ITEM(result, ix, loaded_item);
    }
    return result;

}

PyObject *_loads_tuple(const char *x, int32_t len) {

    if ((uint16_t)len < sizeof(uint16_t)) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }

    uint16_t nstrs;
    memcpy(&nstrs, x, sizeof(uint16_t));

    #if _FOO_KV_DEBUG == 1
    char debug_buffer[256];
    sprintf(debug_buffer, "_loads_tuple(): nstrs=%hu, len=%d", nstrs, len);
    log_debug(debug_buffer);
    #endif

    const char *items[nstrs];
    uint16_t item_to_len[nstrs];
    uint16_t offset = sizeof(uint16_t);

    for (int32_t ix = 0; ix < nstrs; ix++) {
        #if _FOO_KV_DEBUG == 1
        sprintf(debug_buffer, "_loads_tuple(): offset=%hu", offset);
        log_debug(debug_buffer);
        #endif
        // sanity
        if (offset >= len) {
            log_error("_loads_tuple(): got misformed request.");
            _dispatch_errno = RES_ERR_CLIENT;
            return NULL;
        }
        // establish str len
        uint16_t slen;
        memcpy(&slen, x + offset, sizeof(uint16_t));
        if (slen < sizeof(char)) {
            log_error("_loads_tuple(): got request item with not enough size for type indicator");
            _dispatch_errno = RES_ERR_CLIENT;
            return NULL;
        }
        item_to_len[ix] = slen;

        #if _FOO_KV_DEBUG == 1
        sprintf(debug_buffer, "_loads_tuple(): item_to_len[%d]=%d", (int)ix, slen);
        log_debug(debug_buffer);
        #endif

        // establish subcmd
        offset += sizeof(uint16_t);
        items[ix] = x + offset;

        offset += slen;

        #if _FOO_KV_DEBUG == 1
        if (slen < 200) {
            sprintf(debug_buffer, "_loads_tuple(): items[%d]=%.*s", (int)ix, slen, items[ix]);
            log_debug(debug_buffer);
        } else {
            log_debug("_loads_tuple(): subcmd too long for log");
        }
        #endif

    }

    if (offset != len) {
        if (offset > len) {
            log_error("_loads_tuple(): got malformed request: offset overshot len");
        } else {
            log_error("_loads_tuple(): got malformed request: offset undershot len");
        }
        _dispatch_errno = RES_ERR_CLIENT;
        return NULL;
    }

    PyObject *result = PyTuple_New(nstrs);
    for (Py_ssize_t ix = 0; ix < nstrs; ix++) {
        PyObject *loaded_item = _loads_collectable(items[ix], item_to_len[ix]);
        if (!loaded_item) {
            Py_DECREF(result);
            return NULL;
        }
        PyTuple_SET_ITEM(result, ix, loaded_item);
    }
    return result;

}

PyObject *_loads_hashable_tuple(const char *x, int32_t len) {

    if ((uint16_t)len < sizeof(uint16_t)) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }

    uint16_t nstrs;
    memcpy(&nstrs, x, sizeof(uint16_t));

    #if _FOO_KV_DEBUG == 1
    char debug_buffer[256];
    sprintf(debug_buffer, "_loads_hashable_tuple(): nstrs=%hu, len=%d", nstrs, len);
    log_debug(debug_buffer);
    #endif

    const char *items[nstrs];
    uint16_t item_to_len[nstrs];
    uint16_t offset = sizeof(uint16_t);

    for (int32_t ix = 0; ix < nstrs; ix++) {
        #if _FOO_KV_DEBUG == 1
        sprintf(debug_buffer, "_loads_hashable_tuple(): offset=%hu", offset);
        log_debug(debug_buffer);
        #endif
        // sanity
        if (offset >= len) {
            log_error("_loads_hashable_tuple(): got misformed request.");
            _dispatch_errno = RES_ERR_CLIENT;
            return NULL;
        }
        // establish str len
        uint16_t slen;
        memcpy(&slen, x + offset, sizeof(uint16_t));
        if (slen < sizeof(char)) {
            log_error("_loads_hashable_tuple(): got request item with not enough size for type indicator");
            _dispatch_errno = RES_ERR_CLIENT;
            return NULL;
        }
        item_to_len[ix] = slen;

        #if _FOO_KV_DEBUG == 1
        sprintf(debug_buffer, "_loads_hashable_tuple(): item_to_len[%d]=%d", (int)ix, slen);
        log_debug(debug_buffer);
        #endif

        // establish subcmd
        offset += sizeof(uint16_t);
        items[ix] = x + offset;

        offset += slen;

        #if _FOO_KV_DEBUG == 1
        if (slen < 200) {
            sprintf(debug_buffer, "_loads_hashable_tuple(): items[%d]=%.*s", (int)ix, slen, items[ix]);
            log_debug(debug_buffer);
        } else {
            log_debug("_loads_hashable_tuple(): subcmd too long for log");
        }
        #endif

    }

    if (offset != len) {
        if (offset > len) {
            log_error("_loads_hashable_tuple(): got malformed request: offset overshot len");
        } else {
            log_error("_loads_hashable_tuple(): got malformed request: offset undershot len");
        }
        _dispatch_errno = RES_ERR_CLIENT;
        return NULL;
    }

    PyObject *result = PyTuple_New(nstrs);
    for (Py_ssize_t ix = 0; ix < nstrs; ix++) {
        int32_t is_valid = is_hashable(items[ix], item_to_len[ix]);
        if (is_valid < 1) {
            Py_DECREF(result);
            return NULL;
        }
        is_valid = is_collectable(items[ix], item_to_len[ix]);
        if (is_valid < 1) {
            Py_DECREF(result);
            return NULL;
        }
        PyObject *loaded_item = _loads_hashable(items[ix], item_to_len[ix]);
        if (!loaded_item) {
            Py_DECREF(result);
            _dispatch_errno = RES_BAD_HASH;
            return NULL;
        }
        PyTuple_SET_ITEM(result, ix, loaded_item);
    }
    return result;

}

PyObject *_loads_datetime(const char *x, int32_t len) {

    PyObject *xs = PyUnicode_FromStringAndSize(x, len);
    if (!xs) {
        if (PyErr_Occurred()) {
            PyErr_Clear();
        }
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }

    PyObject *res = PyObject_CallMethodObjArgs(_datetime_class, _strptime_str, xs, _datetime_formatstring, NULL);
    Py_DECREF(xs);

    if (!res) {
        if (PyErr_Occurred()) {
            PyErr_Clear();
        }
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }

    return res;

}


PyObject *_loads_from_pyobject(PyObject *x) {

    char *res = PyBytes_AsString(x);
    if (!res) {
        return NULL;
    }

    int32_t len = PyObject_Length(x);
    if (!len) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }

    return loads(res, len);

}


PyObject *_loads_hashable(const char *x, int32_t len) {

    if ((uint32_t)len < sizeof(char)) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }

    switch (x[0]) {
        case INT_SYMBOL:
            return _loads_long(x + 1, len - 1);
        case FLOAT_SYMBOL:
            return _loads_float(x + 1, len - 1);
        case STRING_SYMBOL:
            return _loads_unicode(x + 1, len - 1);
        case BYTES_SYMBOL:
            return PyBytes_FromStringAndSize(x + 1, len - 1);
        case TUPLE_SYMBOL:
            return _loads_hashable_tuple(x + 1, len - 1);
        case LIST_SYMBOL:
        case BOOL_SYMBOL:
        case DATETIME_SYMBOL:
            _dispatch_errno = RES_BAD_HASH;
            return NULL;
        default:
            _dispatch_errno = RES_BAD_TYPE;
            return NULL;
    }

    return NULL;

}


PyObject *_loads_hashable_from_pyobject(PyObject *x) {

    char *res = PyBytes_AsString(x);
    if (!res) {
        return NULL;
    }

    int32_t len = PyObject_Length(x);
    if (!len) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }

    return _loads_hashable(res, len);

}


PyObject *_loads_collectable(const char *x, int32_t len) {

    switch (x[0]) {
        case INT_SYMBOL:
            return _loads_long(x + 1, len - 1);
        case FLOAT_SYMBOL:
            return _loads_float(x + 1, len - 1);
        case STRING_SYMBOL:
            return _loads_unicode(x + 1, len - 1);
        case BYTES_SYMBOL:
            return PyBytes_FromStringAndSize(x + 1, len - 1);
        case LIST_SYMBOL:
            _dispatch_errno = RES_BAD_COLLECTION;
            return NULL;
        case BOOL_SYMBOL:
            return _loads_bool(x + 1, len - 1);
        case DATETIME_SYMBOL:
            return _loads_datetime(x + 1, len - 1);
        default:
            _dispatch_errno = RES_BAD_TYPE;
            return NULL;
    }

    return NULL;

}


PyObject *_loads_collectable_from_pyobject(PyObject *x) {

    char *res = PyBytes_AsString(x);
    if (!res) {
        return NULL;
    }

    int32_t len = PyObject_Length(x);
    if (!len) {
        _dispatch_errno = RES_BAD_COLLECTION;
        return NULL;
    }

    return _loads_collectable(res, len);

}

PyObject *_loads_foo_datetime(const char *x, int32_t len) {

    switch (x[0]) {
        case DATETIME_SYMBOL:
            return _loads_datetime(x + 1, len - 1);
        default:
            _dispatch_errno = RES_BAD_TYPE;
            return NULL;
    }

    return NULL;

}

PyObject *_loads_foo_datetime_from_pyobject(PyObject *x) {

    char *res = PyBytes_AsString(x);
    if (!res) {
        return NULL;
    }

    int32_t len = PyObject_Length(x);
    if (!len) {
        _dispatch_errno = RES_BAD_TYPE;
        return NULL;
    }

    return _loads_datetime(res, len);

}


int32_t is_hashable(const char *x, int32_t len) {

    switch (x[0]) {
        case INT_SYMBOL:
            return 1;
        case FLOAT_SYMBOL:
            return 1;
        case STRING_SYMBOL:
            return 1;
        case BYTES_SYMBOL:
            return 1;
        case LIST_SYMBOL:
            _dispatch_errno = RES_BAD_HASH;
            return 0;
        case BOOL_SYMBOL:
            _dispatch_errno = RES_BAD_HASH;
            return 0;
        case DATETIME_SYMBOL:
            _dispatch_errno = RES_BAD_HASH;
            return 0;
        case TUPLE_SYMBOL:
            return 1;
        default:
            _dispatch_errno = RES_BAD_TYPE;
            return -1;
    }

    _dispatch_errno = RES_UNKNOWN;
    return -1;

}


int32_t is_collectable(const char *x, int32_t len) {

    switch (x[0]) {
        case INT_SYMBOL:
            return 1;
        case FLOAT_SYMBOL:
            return 1;
        case STRING_SYMBOL:
            return 1;
        case BYTES_SYMBOL:
            return 1;
        case LIST_SYMBOL:
            _dispatch_errno = RES_BAD_COLLECTION;
            return 0;
        case BOOL_SYMBOL:
            return 1;
        case DATETIME_SYMBOL:
            return 1;
        case TUPLE_SYMBOL:
            return 1;
        default:
            _dispatch_errno = RES_BAD_TYPE;
            return -1;
    }

    _dispatch_errno = RES_UNKNOWN;
    return -1;

}
