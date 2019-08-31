/**
 *
 *  Copyright 2016-2019 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */

#ifndef LIBVMAF_H_
#define LIBVMAF_H_

#ifndef WINCE
#define TIME_TEST_ENABLE 		1 // 1: memory leak test enable 0: disable
#define MEM_LEAK_TEST_ENABLE 	0 // prints execution time in xml log when enabled.
#else
//For Windows memory leak test and execution time test cases are not handled.
#define TIME_TEST_ENABLE 0
#define MEM_LEAK_TEST_ENABLE 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum VmafLogFmt {
    VMAF_LOG_FMT_XML  = (1 << 0),
    VMAF_LOG_FMT_JSON = (1 << 1),
    VMAF_LOG_FMT_CSV  = (1 << 2),
};

enum VmafPoolingMethod {
    VMAF_POOL_MIN              = (1 << 0),
    VMAF_POOL_MEAN             = (1 << 1),
    VMAF_POOL_HARMONIC_MEAN    = (1 << 2),
};

enum VmafFeatureSetting {
    VMAF_FEATURE_SETTING_DO_NONE       = (1 << 0),
    VMAF_FEATURE_SETTING_DO_PSNR       = (1 << 1),
    VMAF_FEATURE_SETTING_DO_SSIM       = (1 << 2),
    VMAF_FEATURE_SETTING_DO_MS_SSIM    = (1 << 3),
    VMAF_FEATURE_SETTING_DO_COLOR      = (1 << 4),
};

enum VmafPixelFormat {
    VMAF_PIX_FMT_YUV420P         = 0,
    VMAF_PIX_FMT_YUV422P         = 1,
    VMAF_PIX_FMT_YUV444P         = 2,
    VMAF_PIX_FMT_YUV420P10LE     = 3,
    VMAF_PIX_FMT_YUV422P10LE     = 4,
    VMAF_PIX_FMT_YUV444P10LE     = 5,
    VMAF_PIX_FMT_UNKNOWN         = 6,
};

typedef struct VmafPicture
{
    float *data_y;
    float *data_u;
    float *data_v;
//    VmafPixelFormat pix_fmt;
} VmafPicture;

typedef struct {

    int enable_transform;
    int disable_clip;
    int disable_avx;
    int enable_conf_interval;

    int n_thread;
    int n_subsample;

    int width;
    int height;

    char *model_path;
    char *additional_model_paths;

    char *log_path;
    int vmaf_feature_setting;

    enum VmafPixelFormat pix_fmt;
    enum VmafLogFmt log_fmt;
    enum VmafPoolingMethod pool_method;

} VmafContext;

int compute_vmaf(double* vmaf_score,
                 int (*read_frame)(float *ref_data, float *main_data, float *temp_data, int stride_byte, void *user_data),
                 int (*read_vmaf_picture)(VmafPicture *ref_vmaf_pict, VmafPicture *dis_vmaf_pict, float *temp_data, int stride, void *user_data),
				 void *user_data, VmafContext *vmafContext);

#ifdef __cplusplus
}
#endif

#endif /* _LIBVMAF_H */
