cdef extern from "../../../../feature/src/moment_main.c":
     cpdef int run_moment(int order, const char *fmt, const char *video_path, int w, int h)


