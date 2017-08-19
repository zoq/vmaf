cdef extern from "../../../../feature/src/psnr_main.c":
     cpdef int run_psnr(const char *fmt, const char *ref_path, const char *dis_path, int w, int h)


