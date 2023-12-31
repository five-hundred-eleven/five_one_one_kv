#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>

#include "util.h"

// CHANGE ME
#define _FOO_KV_DEBUG 1

// cached python objects
int32_t is_py_deps_init = 0;
PyObject *_builtins = NULL;
PyObject *_asyncio_module = NULL;
PyObject *_queue_class = NULL;
PyObject *_put_nowait_str = NULL;
PyObject *_logging_module = NULL;
PyObject *_logger = NULL;
PyObject *_error_str = NULL;
PyObject *_warning_str = NULL;
PyObject *_info_str = NULL;
PyObject *_debug_str = NULL;
PyObject *_collections_module = NULL;
PyObject *_deq_class = NULL;
PyObject *_pop_str = NULL;
PyObject *_popleft_str = NULL;
PyObject *_append_str = NULL;
PyObject *_threading_module = NULL;
PyObject *_threading_lock = NULL;
PyObject *_threading_cond = NULL;
PyObject *_threading_sem = NULL;
PyObject *_acquire_str = NULL;
PyObject *_release_str = NULL;
PyObject *_locked_str = NULL;
PyObject *_notify_str = NULL;
PyObject *_wait_str = NULL;
PyObject *_acquire_kwargs_noblock = NULL;
PyObject *_acquire_kwargs_timeout = NULL;
PyObject *_acquire_kwargs_block = NULL;
PyObject *_type_f = NULL;
PyObject *_items = NULL;
PyObject *_type_to_symbol = NULL;
PyObject *_string_class = NULL;
PyObject *_list_class = NULL;
PyObject *_int_symbol = NULL;
PyObject *_float_symbol = NULL;
PyObject *_bytes_symbol = NULL;
PyObject *_string_symbol = NULL;
PyObject *_list_symbol = NULL;
PyObject *_tuple_symbol = NULL;
PyObject *_bool_symbol = NULL;
PyObject *_json_kwargs = NULL;
PyObject *_json_loads_f = NULL;
PyObject *_json_dumps_f = NULL;
PyObject *_decode_str = NULL;
PyObject *_utf8_str = NULL;
PyObject *_foo_exceptions_module = NULL;
PyObject *_empty_args = NULL;
PyObject *_embedded_collection_error = NULL;
PyObject *_not_hashable_error = NULL;
PyObject *_datetime_module = NULL;
PyObject *_datetime_class = NULL;
PyObject *_datetime_formatstring = NULL;
PyObject *_datetime_symbol = NULL;
PyObject *_strptime_str = NULL;
PyObject *_strftime_str = NULL;
PyObject *_timestamp_str = NULL;
PyObject *_isinstance_f = NULL;


// error handling
void log_msg(const char *msg, PyObject *method) {
    Py_INCREF(_logger);
    Py_INCREF(method);
    PyObject *py_msg = PyUnicode_FromString(msg);
    PyObject *res = PyObject_CallMethodOneArg(_logger, method, py_msg);
    Py_DECREF(py_msg);
    Py_DECREF(_logger);
    Py_DECREF(method);
    if (!res) {
        fprintf(stderr, "log_msg(): logging failed! msg: %s\n", msg);
        PyObject *py_err = PyErr_Occurred();
        if (py_err) {
            Py_INCREF(_string_class);
            PyObject *py_err_str = PyObject_CallOneArg(_string_class, py_err);
            if (py_err_str) {
                PyObject *py_err_bytes = PyUnicode_AsEncodedString(py_err_str, "utf-8", "ignore");
                Py_DECREF(py_err_str);
                if (py_err_bytes) {
                    const char *err_str = PyBytes_AS_STRING(py_err_bytes);
                    Py_DECREF(py_err_bytes);
                    if (err_str) {
                        fprintf(stderr, "log_msg(): reason for failure: %s\n", err_str);
                    } else {
                        fprintf(stderr, "log_msg(): python error was not castable to string\n");
                    }
                } else {
                    fprintf(stderr, "log_msg(): could not encode reason for failure\n");
                }
                Py_DECREF(_string_class);
                if (PyErr_Occurred()) {
                    PyErr_Clear();
                }
            } else {
                fprintf(stderr, "log_msg(): could not retrieve reason for failure\n");
                if (PyErr_Occurred()) {
                    PyErr_Clear();
                }
            }
        } else {
            fprintf(stderr, "log_msg(): no python error was raised by logger\n");
        }
    }
    Py_DECREF(res);
    return;
}

void log_error(const char *msg) {
    log_msg(msg, _error_str);
}

void log_warning(const char *msg) {
    log_msg(msg, _warning_str);
}

void log_info(const char *msg) {
    log_msg(msg, _info_str);
}

void log_debug(const char *msg) {
    log_msg(msg, _debug_str);
}

void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

// generic
int32_t randint(int32_t min, int32_t max) {

    struct timeval ts;
    gettimeofday(&ts, NULL);
    srand(ts.tv_usec * ts.tv_sec);

    return min + (max - min) * (double)rand() / (double)RAND_MAX + 0.5;

}

int32_t hash_given_len(const uint8_t *s, size_t n) {

    int32_t x = *s << 7;

    for (size_t ix = 0; ix < n; ix++) {
        x = (1000003 * x) ^ s[ix];
    }
    x ^= n;

    if (x == -1) {
        x = -2;
    }

    return x;

}

int32_t hash(const uint8_t *s) {

    int32_t x = *s << 7;
    const uint8_t *p = s;

    while (*p != '\0') {
        x = (1000003 * x) ^ *p++;
    }
    x ^= (p - s);

    if (x == -1) {
        x = -2;
    }

    return x;

}

int32_t read_full(int fd, char *buff, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buff, n);
        if (rv <= 0) {
            // error, or unexpected EOF
            return -1;
        }
        n -= (size_t)rv;
        buff += rv;
    }
    return 0;
}

int32_t write_all(int fd, const char *buff, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buff, n);
        if (rv <= 0) {
            // error
            return -1;
        }
        n -= (size_t)rv;
        buff += rv;
    }
    return 0;
}

// helper for cached python
int32_t ensure_py_deps() {

    if (is_py_deps_init) {
        return 0;
    }

    _builtins = PyEval_GetBuiltins();
    if (_builtins == NULL) {
        return -1;
    }
    _asyncio_module = PyImport_ImportModule("asyncio");
    if (_asyncio_module == NULL) {
        return -1;
    }
    _queue_class = PyObject_GetAttrString(_asyncio_module, "Queue");
    if (_queue_class == NULL) {
        return -1;
    }
    _put_nowait_str = PyUnicode_FromString("put_nowait");
    if (_put_nowait_str == NULL) {
        return -1;
    }
    _logging_module = PyImport_ImportModule("logging");
    if (_logging_module == NULL) {
        return -1;
    }
    PyObject *getLogger = PyObject_GetAttrString(_logging_module, "getLogger");
    if (getLogger == NULL) {
        return -1;
    }
    PyObject *logger_name = PyUnicode_FromString("five_one_one_kv.c.server");
    _logger = PyObject_CallOneArg(getLogger, logger_name);
    Py_DECREF(getLogger);
    Py_DECREF(logger_name);
    if (_logger == NULL) {
        return -1;
    }
    _error_str = PyUnicode_FromString("error");
    if (_error_str == NULL) {
        return -1;
    }
    _warning_str = PyUnicode_FromString("warning");
    if (_warning_str == NULL) {
        return -1;
    }
    _info_str = PyUnicode_FromString("info");
    if (_info_str == NULL) {
        return -1;
    }
    _debug_str = PyUnicode_FromString("debug");
    if (_debug_str == NULL) {
        return -1;
    }
    _collections_module = PyImport_ImportModule("collections");
    if (_collections_module == NULL) {
        return -1;
    }
    _deq_class = PyObject_GetAttrString(_collections_module, "deque");
    if (_deq_class == NULL) {
        return -1;
    }
    _pop_str = PyUnicode_FromString("pop");
    if (_pop_str == NULL) {
        return -1;
    }
    _append_str = PyUnicode_FromString("append");
    if (_append_str == NULL) {
        return -1;
    }
    _popleft_str = PyUnicode_FromString("popleft");
    if (_popleft_str == NULL) {
        return -1;
    }
    _threading_module = PyImport_ImportModule("threading");
    if (_threading_module == NULL) {
        return -1;
    }
    _threading_lock = PyObject_GetAttrString(_threading_module, "Lock");
    if (_threading_lock == NULL) {
        return -1;
    }
    _threading_cond = PyObject_GetAttrString(_threading_module, "Condition");
    if (_threading_cond == NULL) {
        return -1;
    }
    _threading_sem = PyObject_GetAttrString(_threading_module, "Semaphore");
    if (_threading_sem == NULL) {
        return -1;
    }

    _acquire_str = PyUnicode_FromString("acquire");
    if (_acquire_str == NULL) {
        return -1;
    }
    _release_str = PyUnicode_FromString("release");
    if (_release_str == NULL) {
        return -1;
    }
    _locked_str = PyUnicode_FromString("locked");
    if (_locked_str == NULL) {
        return -1;
    }
    _acquire_kwargs_noblock = PyDict_New();
    if (_acquire_kwargs_noblock == NULL) {
        return -1;
    }
    PyDict_SetItemString(_acquire_kwargs_noblock, "blocking", Py_False);
    _acquire_kwargs_timeout = PyDict_New();
    if (_acquire_kwargs_timeout == NULL) {
        return -1;
    }
    PyDict_SetItemString(_acquire_kwargs_timeout, "blocking", Py_True);
    PyObject *py_timeout = PyFloat_FromDouble(1.0);
    PyDict_SetItemString(_acquire_kwargs_timeout, "timeout", py_timeout);
    Py_DECREF(py_timeout);
    _acquire_kwargs_block = PyDict_New();
    if (_acquire_kwargs_block == NULL) {
        return -1;
    }
    PyDict_SetItemString(_acquire_kwargs_block, "blocking", Py_True);
    _notify_str = PyUnicode_FromString("notify");
    if (_notify_str == NULL) {
        return -1;
    }
    _wait_str = PyUnicode_FromString("wait");
    if (_wait_str == NULL) {
        return -1;
    }


    PyObject *separators = PyTuple_New(2);
    if (separators == NULL) {
        return -1;
    }
    PyTuple_SET_ITEM(separators, 0, PyUnicode_FromString(","));
    PyTuple_SET_ITEM(separators, 1, PyUnicode_FromString(":"));
    _json_kwargs = PyDict_New();
    if (_json_kwargs == NULL) {
        return -1;
    }
    PyDict_SetItemString(_json_kwargs, "separators", separators);
    Py_DECREF(separators);

    _type_to_symbol = PyDict_New();
    if (_type_to_symbol == NULL) {
        return -1;
    }
    PyObject *int_class = PyDict_GetItemString(_builtins, "int");
    if (int_class == NULL) {
        return -1;
    }
    PyObject *float_class = PyDict_GetItemString(_builtins, "float");
    if (float_class == NULL) {
        return -1;
    }
    PyObject *bytes_class = PyDict_GetItemString(_builtins, "bytes");
    if (bytes_class == NULL) {
        return -1;
    }
    _string_class = PyDict_GetItemString(_builtins, "str");
    if (_string_class == NULL) {
        return -1;
    }
    _list_class = PyDict_GetItemString(_builtins, "list");
    if (_list_class == NULL) {
        return -1;
    }
    PyObject *tuple_class = PyDict_GetItemString(_builtins, "tuple");
    if (tuple_class == NULL) {
        return -1;
    }
    PyObject *bool_class = PyDict_GetItemString(_builtins, "bool");
    if (bool_class == NULL) {
        return -1;
    }

    _int_symbol = PyBytes_FromFormat("%c", INT_SYMBOL);
    if (_int_symbol == NULL) {
        return -1;
    }
    if (PyDict_SetItem(_type_to_symbol, int_class, _int_symbol)) {
        return -1;
    }
    Py_DECREF(int_class);
    Py_DECREF(_int_symbol);

    _float_symbol = PyBytes_FromFormat("%c", FLOAT_SYMBOL);
    if (_float_symbol == NULL) {
        return -1;
    }
    if (PyDict_SetItem(_type_to_symbol, float_class, _float_symbol)) {
        return -1;
    }
    Py_DECREF(float_class);
    Py_DECREF(_float_symbol);

    _bytes_symbol = PyBytes_FromFormat("%c", BYTES_SYMBOL);
    if (_bytes_symbol == NULL) {
        return -1;
    }
    if (PyDict_SetItem(_type_to_symbol, bytes_class, _bytes_symbol)) {
        return -1;
    }
    Py_DECREF(bytes_class);
    Py_DECREF(_bytes_symbol);

    _string_symbol = PyBytes_FromFormat("%c", STRING_SYMBOL);
    if (_string_symbol == NULL) {
        return -1;
    }
    if (PyDict_SetItem(_type_to_symbol, _string_class, _string_symbol)) {
        return -1;
    }
    Py_DECREF(_string_symbol);

    _list_symbol = PyBytes_FromFormat("%c", LIST_SYMBOL);
    if (_list_symbol == NULL) {
        return -1;
    }
    if (PyDict_SetItem(_type_to_symbol, _list_class, _list_symbol)) {
        return -1;
    }
    Py_DECREF(_list_class);
    Py_DECREF(_list_symbol);

    _tuple_symbol = PyBytes_FromFormat("%c", TUPLE_SYMBOL);
    if (_tuple_symbol == NULL) {
        return -1;
    }
    if (PyDict_SetItem(_type_to_symbol, tuple_class, _tuple_symbol)) {
        return -1;
    }
    Py_DECREF(tuple_class);
    Py_DECREF(_tuple_symbol);

    _bool_symbol = PyBytes_FromFormat("%c", BOOL_SYMBOL);
    if (_bool_symbol == NULL) {
        return -1;
    }
    if (PyDict_SetItem(_type_to_symbol, bool_class, _bool_symbol)) {
        return -1;
    }
    Py_DECREF(bool_class);
    Py_DECREF(_bool_symbol);

    _decode_str = PyUnicode_FromString("decode");
    if (_decode_str == NULL) {
        return -1;
    }

    _utf8_str = PyUnicode_FromString("utf-8");
    if (_utf8_str == NULL) {
        return -1;
    }

    _empty_args = PyTuple_New(0);
    if (_empty_args == NULL) {
        return -1;
    }

    _foo_exceptions_module = PyImport_ImportModule("five_one_one_kv.exceptions");
    if (_foo_exceptions_module == NULL) {
        return -1;
    }

    _embedded_collection_error = PyObject_GetAttrString(_foo_exceptions_module, "EmbeddedCollectionError");
    if (_embedded_collection_error == NULL) {
        return -1;
    }

    _not_hashable_error = PyObject_GetAttrString(_foo_exceptions_module, "NotHashableError");
    if (_not_hashable_error == NULL) {
        return -1;
    }

    _datetime_module = PyImport_ImportModule("datetime");
    if (_datetime_module == NULL) {
        return -1;
    }

    _datetime_class = PyObject_GetAttrString(_datetime_module, "datetime");
    if (_datetime_class == NULL) {
        return -1;
    }

    _datetime_formatstring = PyUnicode_FromString("%Y-%m-%d %H:%M:%S %z");
    if (_datetime_formatstring == NULL) {
        return -1;
    }

    _datetime_symbol = PyBytes_FromFormat("%c", DATETIME_SYMBOL);
    if (_datetime_symbol == NULL) {
        return -1;
    }

    if (PyDict_SetItem(_type_to_symbol, _datetime_class, _datetime_symbol) < 0) {
        return -1;
    }

    _strptime_str = PyUnicode_FromString("strptime");
    if (_strptime_str == NULL) {
        return -1;
    }

    _strftime_str = PyUnicode_FromString("strftime");
    if (_strftime_str == NULL) {
        return -1;
    }

    _timestamp_str = PyUnicode_FromString("timestamp");
    if (_timestamp_str == NULL) {
        return -1;
    }

    is_py_deps_init = 1;

    return 0;

}

int32_t _threading_lock_acquire(PyObject *lock) {

    PyObject *callable = PyObject_GetAttr(lock, _acquire_str);
    if (!callable) {
        return -1;
    }

    PyObject *is_acquired = PyObject_Call(callable, _empty_args, _acquire_kwargs_noblock);
    Py_DECREF(callable);
    if (!is_acquired) {
        return -1;
    }

    int32_t res = Py_IsTrue(is_acquired);
    Py_DECREF(is_acquired);

    return res;

}

int32_t _threading_lock_acquire_block(PyObject *lock) {

    PyObject *callable = PyObject_GetAttr(lock, _acquire_str);
    if (!callable) {
        return -1;
    }

    PyObject *is_acquired = PyObject_Call(callable, _empty_args, _acquire_kwargs_block);
    Py_DECREF(callable);
    if (!is_acquired) {
        return -1;
    }

    int32_t res = Py_IsTrue(is_acquired);
    Py_DECREF(is_acquired);

    return res;

}

int32_t _threading_lock_release(PyObject *lock) {

    PyObject *callable = PyObject_GetAttr(lock, _release_str);
    if (!callable) {
        return -1;
    }

    PyObject *res = PyObject_CallNoArgs(callable);
    Py_DECREF(callable);
    if (!res) {
        return -1;
    }

    Py_DECREF(callable);

    return 0;

}

int32_t _threading_lock_locked(PyObject *lock) {

    PyObject *callable = PyObject_GetAttr(lock, _locked_str);
    if (!callable) {
        return -1;
    }

    PyObject *res = PyObject_CallNoArgs(callable);
    Py_DECREF(callable);
    if (!res) {
        return -1;
    }

    Py_DECREF(callable);

    return Py_IsTrue(res);

}

int32_t _pyobject_safe_delitem(PyObject *obj, PyObject *key) {

    PyObject *py_res;
    Py_INCREF(obj);
    Py_INCREF(_pop_str);
    Py_INCREF(key);
    Py_INCREF(Py_None);

    py_res = PyObject_CallMethodObjArgs(obj, _pop_str, key, Py_None, NULL);
    Py_DECREF(obj);
    Py_DECREF(_pop_str);
    Py_DECREF(key);
    Py_DECREF(Py_None);

    if (!py_res) {
        return -1;
    }

    if (Py_IsNone(py_res)) {
        return 0;
    }

    Py_DECREF(py_res);

    return 1;

}

struct intq_t *intq_new() {
    struct intq_t *intq = PyMem_RawCalloc(1, sizeof(struct intq_t));
    if (!intq) {
        return NULL;
    }
    intq->front = PyMem_RawCalloc(1, sizeof(struct intq_node_t));
    if (!intq->front) {
        return NULL;
    }
    intq->back = intq->front;
    intq->front_ix = 0;
    intq->back_ix = 0;
    return intq;
}

void intq_destroy(struct intq_t *intq) {
    struct intq_node_t *node, *next;
    node = intq->front;
    while (node) {
        next = node->next;
        PyMem_RawFree(node);
        node = next;
    }
    PyMem_RawFree(intq);
}

int32_t intq_put(struct intq_t *intq, int32_t val) {

    if (intq->back_ix >= INTQ_NODE_SIZE) {
        struct intq_node_t *newnode = PyMem_RawCalloc(1, sizeof(struct intq_node_t));
        if (!newnode) {
            return -1;
        }
        intq->back->next = newnode;
        intq->back = newnode;
        intq->back_ix = 0;
    }

    intq->back->vals[intq->back_ix] = val;
    intq->back_ix++;

    return 0;

}

int32_t intq_get(struct intq_t *intq) {

    if (intq_empty(intq)) {
        return -1; // because of this, intq_t only works with positve ints, but that's ok
    }

    int32_t res = intq->front->vals[intq->front_ix];

    intq->front_ix++;
    if (intq->front_ix >= INTQ_NODE_SIZE) {
        if (intq_empty(intq)) {
            intq->front_ix = 0;
            intq->back_ix = 0;
        } else {
            struct intq_node_t *oldnode = intq->front;
            intq->front = intq->front->next;
            PyMem_RawFree(oldnode);
            intq->front_ix = 0;
        }
    }

    return res;

}

struct cond_t *cond_new() {

    struct cond_t *cond = PyMem_RawCalloc(1, sizeof(struct cond_t));
    if (!cond) {
        return NULL;
    }

    if (pthread_cond_init(&cond->cond, NULL)) {
        PyMem_RawFree(cond);
        return NULL;
    }

    if (pthread_mutex_init(&cond->mutex, NULL)) {
        PyMem_RawFree(cond);
        return NULL;
    }

    return cond;

}

void cond_destroy(struct cond_t *cond) {
    pthread_mutex_destroy(&cond->mutex);
    pthread_cond_destroy(&cond->cond);
}

int32_t cond_wait(struct cond_t *cond) {

    int32_t has_lock;

    Py_BEGIN_ALLOW_THREADS
    has_lock = pthread_mutex_lock(&cond->mutex);
    Py_END_ALLOW_THREADS

    if (has_lock) {
        if (has_lock == EDEADLK) {
            log_error("cond_wait(): failed to acquire mutex: thread already owns mutex, will proceed with cond wait");
            // note that we do not return in this case
        } else if (has_lock == EINVAL) {
            log_error("cond_wait(): failed to acquire mutex: mutex does not appear to be valid");
            return -1;
        } else {
            log_error("cond_wait(): failed to acquire mutex: unknown reason");
            return -1;
        }
    }

    // this call will unlock the mutex, wait for the cond, and then re-lock the mutex
    // therefore we need to unlock the mutex again
    Py_BEGIN_ALLOW_THREADS
    has_lock = pthread_cond_wait(&cond->cond, &cond->mutex);
    Py_END_ALLOW_THREADS

    if (has_lock) {
        if (has_lock == EINVAL) {
            log_error("cond_wait() failed to wait for condition: condition does not appear to be valid");
        } else {
            log_error("cond_wait() failed to wait for condition: unknown reason");
        }
        return -1;
    }

    Py_BEGIN_ALLOW_THREADS
    has_lock = pthread_mutex_unlock(&cond->mutex);
    Py_END_ALLOW_THREADS

    if (has_lock) {
        if (has_lock == EBUSY) {
            log_error("cond_wait() failed to unlock mutex: mutex is busy");
        } else if (has_lock == EINVAL) {
            log_error("cond_wait() failed to unlock mutex: mutex does not appear to be initialized");
        } else if (has_lock == EDEADLK) {
            log_error("cond_wait() failed to unlock mutex: mutex already owns the mutex");
        } else if (has_lock == EPERM) {
            log_error("cond_wait() failed to unlock mutex: mutex does not own the mutex");
        } else {
            log_error("cond_wait() failed to unlock mutex: unknown reason");
        }
        return -1;
    }

    return 0;

}

// The following function has different return values than the above
// The following returns 1 if a signal was retrieved, 0 if timeout expired, -1 on error
int32_t cond_timedwait(struct cond_t *cond, struct timespec *ttl) {

    int32_t has_lock, rv, result;

    Py_BEGIN_ALLOW_THREADS
    rv = pthread_mutex_lock(&cond->mutex);
    Py_END_ALLOW_THREADS

    if (rv) {
        if (rv == EDEADLK) {
            log_error("cond_timedwait(): failed to acquire mutex: thread already owns mutex, will proceed with cond wait");
            // note that we do not return in this case
        } else if (rv == EINVAL) {
            log_error("cond_timedwait(): failed to acquire mutex: mutex does not appear to be valid");
            return -1;
        } else {
            log_error("cond_timedwait(): failed to acquire mutex: unknown reason");
            return -1;
        }
    }

    // this call will unlock the mutex, wait for the cond, and then re-lock the mutex
    // therefore we need to unlock the mutex again
    Py_BEGIN_ALLOW_THREADS
    has_lock = pthread_cond_timedwait(&cond->cond, &cond->mutex, ttl);
    Py_END_ALLOW_THREADS

    if (has_lock) {
        if (has_lock == ETIMEDOUT) {
            #if _FOO_KV_DEBUG == 1
            log_debug("cond_timedwait(): condition timed out: perhaps this is expected");
            #endif
            result = 0;
        } else if (has_lock == EINVAL) {
            log_error("cond_timedwait() failed to wait for condition: condition does not appear to be valid");
            return -1;
        } else {
            log_error("cond_timedwait() failed to wait for condition: unknown reason");
            return -1;
        }
    } else {
        result = 1;
    }

    Py_BEGIN_ALLOW_THREADS
    rv = pthread_mutex_unlock(&cond->mutex);
    Py_END_ALLOW_THREADS

    if (rv) {
        if (rv == EBUSY) {
            log_error("cond_timedwait() failed to unlock mutex: mutex is busy");
        } else if (rv == EINVAL) {
            log_error("cond_timedwait() failed to unlock mutex: mutex does not appear to be initialized");
        } else if (rv == EDEADLK) {
            log_error("cond_timedwait() failed to unlock mutex: mutex already owns the mutex");
        } else if (rv == EPERM) {
            log_error("cond_timedwait() failed to unlock mutex: mutex does not own the mutex");
        } else {
            log_error("cond_timedwait() failed to unlock mutex: unknown reason");
        }
        return -1;
    }

    return result;

}


int32_t cond_notify(struct cond_t *cond) {

    int32_t res;

    Py_BEGIN_ALLOW_THREADS
    res = pthread_cond_signal(&cond->cond);
    Py_END_ALLOW_THREADS

    if (res) {
        if (errno == EINVAL) {
            log_error("cond_notify() failed to signal condition: cond does not appear to be initialized");
        } else {
            log_error("cond_notify() failed to signal condition: unknown reason");
        }
        return -1;
    }

    return 0;

}

int32_t threadsafe_sem_wait(sem_t *sem) {

    int32_t res;

    Py_BEGIN_ALLOW_THREADS
    res = sem_wait(sem);
    Py_END_ALLOW_THREADS

    return res;

}

int32_t threadsafe_sem_timedwait_onesec(sem_t *sem) {

    int32_t res;
    struct timespec sem_timeout;
    sem_timeout.tv_sec = 1;
    sem_timeout.tv_nsec = 0;

    Py_BEGIN_ALLOW_THREADS
    res = sem_timedwait(sem, &sem_timeout);
    Py_END_ALLOW_THREADS

    return res;

}
