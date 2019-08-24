#pragma once

#include <Python.h>

class GIL_guard {
public:
    GIL_guard();
    ~GIL_guard();

private:
    PyGILState_STATE state;
};

class PyThread_guard {
public:
    PyThread_guard();
    ~PyThread_guard();

private:
    PyThreadState *state;
};
