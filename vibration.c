#include <Python.h>
#include <math.h>

static int require_sequence(PyObject *obj) {
    if (PyList_Check(obj) || PyTuple_Check(obj))
        return 1;
    PyErr_SetString(PyExc_TypeError, "expected list or tuple of floats");
    return 0;
}

static int get_float_item(PyObject *seq, Py_ssize_t i, double *out) {
    PyObject *item = PySequence_GetItem(seq, i);
    if (!item)
        return 0;
    if (!PyFloat_Check(item)) {
        Py_DECREF(item);
        PyErr_SetString(PyExc_TypeError, "all elements must be float");
        return 0;
    }
    *out = PyFloat_AS_DOUBLE(item);
    Py_DECREF(item);
    return 1;
}

static PyObject *peak_to_peak(PyObject *self, PyObject *args) {
    (void)self;
    PyObject *data;
    if (!PyArg_ParseTuple(args, "O", &data))
        return NULL;
    if (!require_sequence(data))
        return NULL;

    Py_ssize_t n = PySequence_Size(data);
    if (n == 0)
        return PyFloat_FromDouble(0.0);

    double v, lo, hi;
    if (!get_float_item(data, 0, &v))
        return NULL;
    lo = hi = v;
    for (Py_ssize_t i = 1; i < n; i++) {
        if (!get_float_item(data, i, &v))
            return NULL;
        if (v < lo)
            lo = v;
        if (v > hi)
            hi = v;
    }
    return PyFloat_FromDouble(hi - lo);
}

static PyObject *rms(PyObject *self, PyObject *args) {
    (void)self;
    PyObject *data;
    if (!PyArg_ParseTuple(args, "O", &data))
        return NULL;
    if (!require_sequence(data))
        return NULL;

    Py_ssize_t n = PySequence_Size(data);
    if (n == 0)
        return PyFloat_FromDouble(0.0);

    double sumsq = 0.0, v;
    for (Py_ssize_t i = 0; i < n; i++) {
        if (!get_float_item(data, i, &v))
            return NULL;
        sumsq += v * v;
    }
    return PyFloat_FromDouble(sqrt(sumsq / (double)n));
}

static PyObject *std_dev(PyObject *self, PyObject *args) {
    (void)self;
    PyObject *data;
    if (!PyArg_ParseTuple(args, "O", &data))
        return NULL;
    if (!require_sequence(data))
        return NULL;

    Py_ssize_t n = PySequence_Size(data);
    if (n < 2)
        return PyFloat_FromDouble(0.0);

    double sum = 0.0, v;
    for (Py_ssize_t i = 0; i < n; i++) {
        if (!get_float_item(data, i, &v))
            return NULL;
        sum += v;
    }
    double mean = sum / (double)n;

    double varsum = 0.0;
    for (Py_ssize_t i = 0; i < n; i++) {
        if (!get_float_item(data, i, &v))
            return NULL;
        double d = v - mean;
        varsum += d * d;
    }
    return PyFloat_FromDouble(sqrt(varsum / (double)(n - 1)));
}

static PyObject *above_threshold(PyObject *self, PyObject *args) {
    (void)self;
    PyObject *data;
    double threshold;
    if (!PyArg_ParseTuple(args, "Od", &data, &threshold))
        return NULL;
    if (!require_sequence(data))
        return NULL;

    Py_ssize_t n = PySequence_Size(data);
    long count = 0;
    double v;
    for (Py_ssize_t i = 0; i < n; i++) {
        if (!get_float_item(data, i, &v))
            return NULL;
        if (v > threshold)
            count++;
    }
    return PyLong_FromLong(count);
}

static PyObject *summary(PyObject *self, PyObject *args) {
    (void)self;
    PyObject *data;
    if (!PyArg_ParseTuple(args, "O", &data))
        return NULL;
    if (!require_sequence(data))
        return NULL;

    Py_ssize_t n = PySequence_Size(data);
    if (n == 0)
        return Py_BuildValue("{s:i,s:d,s:d,s:d}", "count", 0, "mean", 0.0, "min", 0.0, "max", 0.0);

    double sum = 0.0, v, lo, hi;
    if (!get_float_item(data, 0, &v))
        return NULL;
    lo = hi = v;
    sum = v;
    for (Py_ssize_t i = 1; i < n; i++) {
        if (!get_float_item(data, i, &v))
            return NULL;
        sum += v;
        if (v < lo)
            lo = v;
        if (v > hi)
            hi = v;
    }
    return Py_BuildValue("{s:n,s:d,s:d,s:d}", "count", n, "mean", sum / (double)n, "min", lo, "max", hi);
}

static PyMethodDef methods[] = {
    {"peak_to_peak", (PyCFunction)peak_to_peak, METH_VARARGS, "max - min"},
    {"rms", (PyCFunction)rms, METH_VARARGS, "sqrt(mean(x^2))"},
    {"std_dev", (PyCFunction)std_dev, METH_VARARGS, "sample standard deviation"},
    {"above_threshold", (PyCFunction)above_threshold, METH_VARARGS, "count strictly above threshold"},
    {"summary", (PyCFunction)summary, METH_VARARGS, "count, mean, min, max"},
    {NULL, NULL, 0, NULL}};

static struct PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "vibration",
    .m_doc = "Vibration statistics (double precision in C)",
    .m_size = -1,
    .m_methods = methods,
};

PyMODINIT_FUNC PyInit_vibration(void) {
    return PyModule_Create(&module);
}
