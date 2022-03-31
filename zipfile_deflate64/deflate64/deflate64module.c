#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>

// "zutil.h" has necessary #defines for "infback9.h"
#include "zutil.h"
#include "infback9.h"

typedef struct {
    PyObject_HEAD
    z_stream* strm;
    PyObject* window_buffer;
    PyObject* output_buffer;
    char eof;
} Deflate64Object;

static voidpf zlib_alloc(voidpf opaque, uInt items, uInt size) {
    // For safety, give zlib a zero-initialized memory block
    // Also, PyMem_Calloc call does an overflow-safe maximum size check
    void* address = PyMem_Calloc(items, size);
    if (address == NULL) {
        // For safety, don't assume Z_NULL is the same as NULL
        return Z_NULL;
    }

    return address;
}

static void zlib_free(voidpf opaque, voidpf address) {
    PyMem_Free(address);
}

static int Deflate64_init(Deflate64Object* self, PyObject* args, PyObject* kwds) {
    self->strm = PyMem_Calloc(1, sizeof(z_stream));
    if (self->strm == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    self->strm->opaque = NULL;
    self->strm->zalloc = zlib_alloc;
    self->strm->zfree = zlib_free;

    // infback9 requires that the window buffer be exactly 64K
    self->window_buffer = PyBytes_FromStringAndSize(NULL, 64 << 10);
    if (self->window_buffer == NULL) {
        PyErr_NoMemory();
        return -1;
    }

    int err = inflate9Init(self->strm, (unsigned char*) PyBytes_AS_STRING(self->window_buffer));
    switch (err) {
        case Z_OK:
            // Success
            break;
        case Z_MEM_ERROR:
            // The internal state could not be allocated
            PyErr_NoMemory();
            return -1;
        // Fatal errors
        case Z_STREAM_ERROR:
            // Some parameters are invalid
        case Z_VERSION_ERROR:
            // The version of the library does not match the version of the header file
        default:
            PyErr_BadInternalCall();
            return -1;
    }

    // Default eof to false
    self->eof = 0;

    return 0;
}

static void Deflate64_dealloc(Deflate64Object* self) {
    if (self->strm != NULL) {
        int err = inflate9End(self->strm);
        switch (err) {
            case Z_OK:
                // Success
                break;
            // Fatal errors
            case Z_STREAM_ERROR:
                // Some parameters are invalid
            default:
                PyErr_BadInternalCall();
                break;
        }
    }
    Py_XDECREF(self->window_buffer);
    PyMem_Free(self->strm);

    Py_TYPE(self)->tp_free((PyObject*) self);
}

static PyObject* Deflate64_decompress(Deflate64Object* self, PyObject *args) {
    PyObject* ret = NULL;

    Py_buffer input_buffer;
    if (!PyArg_ParseTuple(args, "y*", &input_buffer)) {
        return NULL;
    }

    // Allocate now, but with no size; this will be resized later
    char *output_buffer = malloc(0);
    if (output_buffer == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    PyThreadState *_save;
    _save = PyEval_SaveThread();

    size_t total_out = 0;
    const int bufsize = 2048;
    Bytef next_out[bufsize];
    self->strm->avail_out = 0;
    self->strm->next_in = input_buffer.buf;
    self->strm->avail_in = (uInt) input_buffer.len;

    for(;;){
        if(self->strm->avail_out==0){
            self->strm->avail_out=bufsize;
            self->strm->next_out=next_out;
        }
        int prev_avail_out = self->strm->avail_out;
        Bytef *prev_next_out = self->strm->next_out;
        Bytef *prev_next_in = self->strm->next_in;
        int err = inflate9(self->strm);
        switch (err) {
            case Z_OK:
                // Success
                break;
            case Z_STREAM_END:
                // Success
                self->eof = 1;
                break;
            case Z_DATA_ERROR:
                // Deflate format error
                PyEval_RestoreThread(_save);
                PyErr_Format(PyExc_ValueError, "Bad Deflate64 data: %s", self->strm->msg);
                goto error;
            case Z_MEM_ERROR:
                // Could not allocate memory for the state
                PyEval_RestoreThread(_save);
                PyErr_NoMemory();
                goto error;
            // Fatal errors
            case Z_STREAM_ERROR:
                // Some parameters are invalid
            default:
                PyEval_RestoreThread(_save);
                PyErr_BadInternalCall();
                goto error;
        }
        if(prev_next_in==self->strm->next_in && prev_next_out==self->strm->next_out)break;

        int len = prev_avail_out - self->strm->avail_out;
        if(len){
            // Concatenate buf onto self->output_buffer
            size_t old_output_size = total_out; //PyBytes_GET_SIZE(self->output_buffer);
            total_out += len;
            output_buffer = realloc(output_buffer, old_output_size + len);
            if(output_buffer == NULL){
                PyEval_RestoreThread(_save);
                PyErr_NoMemory();
                goto error;
            }

            char* output_dest = output_buffer + old_output_size;

            memcpy(output_dest, prev_next_out, len);

        }
        if(err==Z_STREAM_END)break;
    }

    PyEval_RestoreThread(_save);
    self->output_buffer = PyBytes_FromStringAndSize(output_buffer, total_out);
    ret = self->output_buffer;

error:
    free(output_buffer);
    PyBuffer_Release(&input_buffer);
    return ret;
}

static PyMemberDef Deflate64_members[] = {
    {
        .name = "eof",
        .type = T_BOOL,
        .offset = offsetof(Deflate64Object, eof),
        .flags = READONLY,
        .doc = "end of file"
    },
    {NULL}
};

static PyMethodDef Deflate64_methods[] = {
    {"decompress", (PyCFunction) Deflate64_decompress, METH_VARARGS, "Decompress a Deflate64 stream."},
    {NULL}
};

static PyTypeObject Deflate64_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "zipfile_deflate64.deflate64.Deflate64",
    .tp_doc = "An object for Deflate64 decompression.",
    .tp_basicsize = sizeof(Deflate64Object),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc) Deflate64_init,
    .tp_dealloc = (destructor) Deflate64_dealloc,
    .tp_members = Deflate64_members,
    .tp_methods = Deflate64_methods,
};

static PyModuleDef deflate64_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "deflate64",
    .m_doc = "Python access to zlib's infback9 extension for Deflate64 decompression.",
    .m_size = -1,
};

PyMODINIT_FUNC PyInit_deflate64(void) {
    PyObject* m = PyModule_Create(&deflate64_module);
    if (m == NULL) {
        return NULL;
    }

    if (PyType_Ready(&Deflate64_type) < 0) {
        Py_DECREF(m);
        return NULL;
    }

    Py_INCREF(&Deflate64_type);
    if (PyModule_AddObject(m, "Deflate64", (PyObject*) &Deflate64_type) < 0) {
        Py_DECREF(&Deflate64_type);
        Py_DECREF(m);
        return NULL;
    }

    return m;
}
