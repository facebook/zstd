/**
 * Copyright (c) 2016-present, Dima Krasner
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <zstd.h>
#include <Python.h>

static PyObject *ZstdError;

static PyObject*
zstd_compress(PyObject* self, PyObject* args, PyObject* kwargs)
{
    static char *kwds[] = {"buf", "level", NULL};
    PyObject *str;
    const char *in;
    char *out;
    int inlen, level = 5;
    size_t outlen, buflen;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s#|i", kwds, &in, &inlen, &level))
        return NULL;

    buflen = ZSTD_compressBound((size_t)inlen);
    out = (char *)malloc(buflen);
    if (!out)
        return PyErr_NoMemory();

    Py_BEGIN_ALLOW_THREADS
    outlen = ZSTD_compress(out, buflen, in, inlen, level);
    Py_END_ALLOW_THREADS

    if (ZSTD_isError(outlen)) {
        free(out);
        PyErr_SetString(ZstdError, ZSTD_getErrorName(outlen));
        return NULL;
    }

    out[outlen] = '\0';
    str = PyBytes_FromStringAndSize(out, (Py_ssize_t)outlen);
    free(out);
    if (!str)
        PyErr_SetNone(ZstdError);
    return str;
}

PyDoc_STRVAR(compress_doc,
"compress(string, level=5) -> str\n\n\
Compress a buffer using Zstandard.");

static PyObject*
zstd_decompress(PyObject* self, PyObject* args, PyObject* kwargs)
{
    static char *kwds[] = {"buf", "size", NULL};
    PyObject *str;
    const char *in;
    char *out;
    int inlen;
    size_t outlen;
    unsigned long long buflen = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s#|K", kwds, &in, &inlen, &buflen))
        return NULL;

    if (!buflen) {
        buflen = ZSTD_getDecompressedSize(in, inlen);
        if (!buflen) {
            PyErr_SetString(ZstdError, "Cannot guess decompressed size");
            return NULL;
        }
    }

    out = (char *)malloc((size_t)buflen);
    if (!out)
        return PyErr_NoMemory();

    Py_BEGIN_ALLOW_THREADS
    outlen = ZSTD_decompress(out, (size_t)buflen, in, inlen);
    Py_END_ALLOW_THREADS

    if (ZSTD_isError(outlen)) {
        free(out);
        PyErr_SetString(ZstdError, ZSTD_getErrorName(outlen));
        return NULL;
    }

    out[outlen] = '\0';
    str = PyBytes_FromStringAndSize(out, (Py_ssize_t)outlen);
    free(out);
    if (!str)
        PyErr_SetNone(ZstdError);
    return str;
}

PyDoc_STRVAR(decompress_doc,
"decompress(string, size=0) -> str\n\n\
Decompress a Zstandard-compressed buffer. If decompressed size is unspecified or zero, try to guess it.");

static PyMethodDef ZstdMethods[] = {
    {"compress", (PyCFunction)zstd_compress, METH_VARARGS | METH_KEYWORDS, compress_doc},
    {"decompress", (PyCFunction)zstd_decompress, METH_VARARGS | METH_KEYWORDS, decompress_doc},
    {NULL, NULL, 0, NULL}
};

#if PY_MAJOR_VERSION >= 3

static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "zstd",
    0,
    -1,
    ZstdMethods,
    NULL,
    NULL,
    NULL,
    NULL
};
#endif

PyMODINIT_FUNC
#if PY_MAJOR_VERSION >= 3
PyInit_zstd(void)
#else
initzstd(void)
#endif
{
    PyObject *m, *max;

#if PY_MAJOR_VERSION >= 3
    m = PyModule_Create(&moduledef);
    if (!m)
        return NULL;
#else
    m = Py_InitModule("zstd", ZstdMethods);
    if (!m)
        return;
#endif

#if PY_MAJOR_VERSION >= 3
    max = PyLong_FromLong((long)ZSTD_maxCLevel());
    if (!max)
        return NULL;
#else
    max = PyInt_FromLong((long)ZSTD_maxCLevel());
    if (!max)
        return;
#endif

    Py_INCREF(max);
    PyModule_AddObject(m, "ZSTD_BEST_COMPRESSION", max);

    ZstdError = PyErr_NewException("zstd.error", NULL, NULL);
    if (!ZstdError)
#if PY_MAJOR_VERSION >= 3
        return NULL;
#else
        return;
#endif

    Py_INCREF(ZstdError);
    PyModule_AddObject(m, "error", ZstdError);

#if PY_MAJOR_VERSION >= 3
    return m;
#endif
}
