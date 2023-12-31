#ifndef _FOO_KV_UTIL
#define _FOO_KV_UTIL

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>

#include <Python.h>

#define DEFAULT_MSG_SIZE 4096
#define MAX_MSG_SIZE 65535  // 2**16 - 1
#define MAX_KEY_SIZE 1024  // not currently enforced
#define MAX_VAL_SIZE MAX_MSG_SIZE - 2048  // not currently enforced
#define CEIL(x, y) ((y) * ((x) / (y) + ((x) % (y) != 0)))

#define INTQ_NODE_SIZE 16

// cached python objects
extern int32_t is_py_deps_init;
extern PyObject *_builtins;
extern PyObject *_asyncio_module;
extern PyObject *_queue_class;
extern PyObject *_put_nowait_str;
extern PyObject *_logging_module;
extern PyObject *_logger;
extern PyObject *_error_str;
extern PyObject *_warning_str;
extern PyObject *_info_str;
extern PyObject *_debug_str;
extern PyObject *_collections_module;
extern PyObject *_deq_class;
extern PyObject *_pop_str;
extern PyObject *_append_str;
extern PyObject *_push_str;
extern PyObject *_popleft_str;
extern PyObject *_json_module;
extern PyObject *_threading_module;
extern PyObject *_threading_lock;
extern PyObject *_threading_cond;
extern PyObject *_threading_sem;
extern PyObject *_acquire_str;
extern PyObject *_release_str;
extern PyObject *_locked_str;
extern PyObject *_notify_str;
extern PyObject *_wait_str;
extern PyObject *_acquire_kwargs_noblock;
extern PyObject *_acquire_kwargs_timeout;
extern PyObject *_acquire_kwargs_block;
extern PyObject *_type_f;
extern PyObject *_items;
extern PyObject *_type_to_symbol;
extern PyObject *_string_class;
extern PyObject *_list_class;
extern PyObject *_int_symbol;
extern PyObject *_float_symbol;
extern PyObject *_bytes_symbol;
extern PyObject *_string_symbol;
extern PyObject *_list_symbol;
extern PyObject *_tuple_symbol;
extern PyObject *_bool_symbol;
extern PyObject *_json_kwargs;
extern PyObject *_json_loads_f;
extern PyObject *_json_dumps_f;
extern PyObject *_decode_str;
extern PyObject *_utf8_str;
extern PyObject *_empty_args;
extern PyObject *_foo_exceptions_module;
extern PyObject *_embedded_collection_error;
extern PyObject *_not_hashable_error;
extern PyObject *_datetime_module;
extern PyObject *_datetime_class;
extern PyObject *_datetime_formatstring;
extern PyObject *_strptime_str;
extern PyObject *_strftime_str;
extern PyObject *_timestamp_str;
extern PyObject *_isinstance_f;

#define INT_SYMBOL '#'
#define FLOAT_SYMBOL '%'
#define BYTES_SYMBOL '\''
#define STRING_SYMBOL '"'
#define LIST_SYMBOL '['
#define TUPLE_SYMBOL '('
#define BOOL_SYMBOL '?'
#define DATETIME_SYMBOL '+'

// this sets the above cached objects
int32_t ensure_py_deps();
// threading utils
int32_t _threading_lock_acquire(PyObject *lock);
int32_t _threading_lock_acquire_block(PyObject *lock);
int32_t _threading_lock_release(PyObject *lock);
int32_t _threading_lock_locked(PyObject *lock);
int32_t _pyobject_safe_delitem(PyObject *obj, PyObject *key);

// server connection states
enum {
    STATE_REQ = 0,
    STATE_RES = 1,
    STATE_DISPATCH = 2,
    STATE_REQ_WAITING = 3,
    STATE_RES_WAITING = 4,
    STATE_END = 5,
    STATE_TERM = 6,
};

// expected result
#define RES_OK 0 
// catch-all for unknown errors
#define RES_UNKNOWN 11 
// server messed up
#define RES_ERR_SERVER 21 
// server blames client
#define RES_ERR_CLIENT 22 
// command not found
#define RES_BAD_CMD 31 
// type not found
#define RES_BAD_TYPE 32 
// key not found
#define RES_BAD_KEY 33 
// bad args for the command
#define RES_BAD_ARGS 34 
// bad operation, ex. INC on not int
#define RES_BAD_OP 35 
// index out of bound for list/queue commands
#define RES_BAD_IX 36 
// index out of bound for list/queue commands
#define RES_BAD_HASH 37
// embedded collection
#define RES_BAD_COLLECTION 38

// basic utils
void log_error(const char *msg);
void log_warning(const char *msg);
void log_info(const char *msg);
void log_debug(const char *msg);
void die(const char *msg);
int32_t randint(int32_t min, int32_t max);
int32_t hash_given_len(const uint8_t *s, size_t n);
int32_t hash(const uint8_t *s);

// read/write
int32_t read_full(int fd, char *buff, size_t n); 
int32_t write_all(int fd, const char *buff, size_t n);

struct intq_node_t {
    struct intq_node_t *next;
    int32_t vals[INTQ_NODE_SIZE];
};

struct intq_t {
    struct intq_node_t *front;
    struct intq_node_t *back;
    int32_t front_ix;
    int32_t back_ix;
};

struct intq_t *intq_new();
void intq_destroy(struct intq_t *intq);
#define intq_empty(intq) ((intq->front == intq->back) && (intq->front_ix == intq->back_ix))
int32_t intq_put(struct intq_t *intq, int32_t val);
int32_t intq_get(struct intq_t *intq);

struct cond_t {
    pthread_cond_t cond;
    pthread_mutex_t mutex;
};

struct cond_t *cond_new();
void cond_destroy(struct cond_t *cond);
int32_t cond_wait(struct cond_t *cond);
int32_t cond_timedwait(struct cond_t *cond, struct timespec *ttl);
int32_t cond_notify(struct cond_t *cond);

// threadsafe wrappers for sem
int32_t threadsafe_sem_wait(sem_t *sem);
int32_t threadsafe_sem_timedwait_onesec(sem_t *sem);

#endif
