from libcpp cimport bool
cdef extern from "../../../../wrapper/src/main.cpp":
     cpdef int run_wrapper(char *fmt, int width, int height, char *ref_path, char *dis_path, char *model_path, char *log_path, char *log_fmt, bool disable_clip, bool disable_avx, bool enable_transform, bool phone_model, bool do_psnr, bool do_ssim, bool do_ms_ssim, char *pool_method)


