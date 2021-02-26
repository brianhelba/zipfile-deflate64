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

    int err = inflateBack9Init(self->strm, (unsigned char*) PyBytes_AS_STRING(self->window_buffer));
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

    // Allocate now, but with no size; this will be resized later
    self->output_buffer = PyBytes_FromStringAndSize(NULL, 0);
    if (self->output_buffer == NULL) {
        PyErr_NoMemory();
        return -1;
    }

    // Default eof to false
    self->eof = 0;

    return 0;
}

static void Deflate64_dealloc(Deflate64Object* self) {
    Py_XDECREF(self->output_buffer);
    if (self->strm != NULL) {
        int err = inflateBack9End(self->strm);
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

static unsigned zlib_in(void* in_desc, z_const unsigned char** buf) {
    // Input from input_buffer is set before calling inflateBack9
    // No additional input is ever available
    return 0;
}

static int zlib_out(void* out_desc, unsigned char* buf, unsigned len) {
    Deflate64Object* self = (Deflate64Object*) out_desc;

    // Concatenate buf onto self->output_buffer
    Py_ssize_t old_output_size = PyBytes_GET_SIZE(self->output_buffer);

#if PY_VERSION_HEX < 0x3070700 // v3.7.3
    // Workaround for bpo-33817, which was first (via backport) fixed in Python 3.7.3
    // Before this, size-zero bytes objects could not be resized
    if (old_output_size == 0) {
        Py_DECREF(self->output_buffer);
        // Just create a new buffer with the target size; the following resize will short-circuit
        self->output_buffer = PyBytes_FromStringAndSize(NULL, old_output_size + len);
        if (self->output_buffer == NULL) {
            PyErr_NoMemory();
            return -1;
        }
    }
#endif

    int err = _PyBytes_Resize(&self->output_buffer, old_output_size + len);
    if (err < 0) {
        // MemoryError is set, and output_buffer is deallocated and set to NULL
        return -1;
    }

    char* output_dest = PyBytes_AS_STRING(self->output_buffer) + old_output_size;

    memcpy(output_dest, buf, len);

    return 0;
}

static PyObject* Deflate64_decompress(Deflate64Object* self, PyObject *args) {
    PyObject* ret = NULL;

    Py_buffer input_buffer;
    if (!PyArg_ParseTuple(args, "y*", &input_buffer)) {
        return NULL;
    }

    self->strm->next_in = input_buffer.buf;
    self->strm->avail_in = (uInt) input_buffer.len;

    int err = inflateBack9(self->strm, &zlib_in, self, &zlib_out, self);
    switch (err) {
        case Z_STREAM_END:
            // Success
            self->eof = 1;
            break;
        case Z_BUF_ERROR:
            // in() or out() returned an error
            if (self->strm->next_in == Z_NULL) {
                // in() ran out of input; this is a somewhat expected condition
                self->eof = 0;
                break;
            } else {
                // out() returned an error; it's expected that it already set a PyErr
                goto error;
            }
            break;
        case Z_DATA_ERROR:
            // Deflate format error
            PyErr_Format(PyExc_ValueError, "Bad Deflate64 data: %s", self->strm->msg);
            goto error;
        case Z_MEM_ERROR:
            // Could not allocate memory for the state
            PyErr_NoMemory();
            goto error;
        // Fatal errors
        case Z_STREAM_ERROR:
            // Some parameters are invalid
        default:
            PyErr_BadInternalCall();
            goto error;
    }

    // This method returns a new reference to output_buffer, which should persist even after this
    // object is deallocated (which decrements output_buffer)
    Py_INCREF(self->output_buffer);
    ret = self->output_buffer;

error:
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
