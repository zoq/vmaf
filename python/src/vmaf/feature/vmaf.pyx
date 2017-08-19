from libcpp cimport bool
cdef extern from "../../../../feature/src/vmaf_main.c":
     cpdef int run_vmaf(const char *app, const char *fmt, const char *ref_path, const char *dis_path, int w, int h)



