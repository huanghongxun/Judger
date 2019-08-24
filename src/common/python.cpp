#include "common/python.hpp"

GIL_guard::GIL_guard() {
    state = PyGILState_Ensure();
}

GIL_guard::~GIL_guard() {
    PyGILState_Release(state);
}

PyThread_guard::PyThread_guard() {
    state = PyEval_SaveThread();
}

PyThread_guard::~PyThread_guard() {
    PyEval_RestoreThread(state);
}