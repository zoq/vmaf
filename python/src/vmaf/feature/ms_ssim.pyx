cdef extern from "../../../../feature/src/ms_ssim_main.c":
     cpdef int run_ms_ssim(const char *fmt, const char *ref_path, const char *dis_path, int w, int h)


