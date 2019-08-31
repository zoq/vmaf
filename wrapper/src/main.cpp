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

#include <cstdio>
#include <stdlib.h>
#include <stdexcept>
#include <exception>
#include <string>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <sys/stat.h>

#include "libvmaf.h"

extern "C" {
#include "common/frame.h"
}

#define read_image_b       read_image_b2s
#define read_image_w       read_image_w2s

char* getCmdOption(char ** begin, char ** end, const std::string & option)
{
    char ** itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        return *itr;
    }
    return 0;
}

bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}

void print_usage(int argc, char *argv[])
{
    fprintf(stderr, "Usage: %s fmt width height ref_path dis_path model_path [--log log_path] [--log-fmt log_fmt] [--thread n_thread] [--subsample n_subsample] [--disable-clip] [--disable-avx] [--psnr] [--ssim] [--ms-ssim] [--phone-model] [--ci]\n", argv[0]);
    fprintf(stderr, "fmt:\n\tyuv420p\n\tyuv422p\n\tyuv444p\n\tyuv420p10le\n\tyuv422p10le\n\tyuv444p10le\n\n");
    fprintf(stderr, "log_fmt:\n\txml (default)\n\tjson\n\tcsv\n\n");
    fprintf(stderr, "n_thread:\n\tmaximum threads to use (default 0 - use all threads)\n\n");
    fprintf(stderr, "n_subsample:\n\tn indicates computing on one of every n frames (default 1)\n\n");
}

#if MEM_LEAK_TEST_ENABLE
/*
 * Measures the current (and peak) resident and virtual memories
 * usage of your linux C process, in kB
 */
void getMemory(int itr_ctr, int state)
{
	int currRealMem;
	int peakRealMem;
	int currVirtMem;
	int peakVirtMem;
	char state_str[10]="";
    // stores each word in status file
    char buffer[1024] = "";
	
	if(state ==1)
		strcpy(state_str,"start");
	else
		strcpy(state_str,"end");
		
    // linux file contains this-process info
    FILE* file = fopen("/proc/self/status", "r");

    // read the entire file
    while (fscanf(file, " %1023s", buffer) == 1)
	{
        if (strcmp(buffer, "VmRSS:") == 0)
		{
            fscanf(file, " %d", &currRealMem);
        }
        if (strcmp(buffer, "VmHWM:") == 0)
		{
            fscanf(file, " %d", &peakRealMem);
        }
        if (strcmp(buffer, "VmSize:") == 0)
		{
            fscanf(file, " %d", &currVirtMem);
        }
        if (strcmp(buffer, "VmPeak:") == 0)
		{
            fscanf(file, " %d", &peakVirtMem);
        }
    }
    fclose(file);
    printf("Iteration %d at %s of process: currRealMem: %6d, peakRealMem: %6d, currVirtMem: %6d, peakVirtMem: %6d\n",itr_ctr, state_str, currRealMem, peakRealMem, currVirtMem, peakVirtMem);
}
#endif

int run_wrapper(enum VmafPixelFormat pix_fmt, int width, int height, char *ref_path, char *dis_path, char *model_path, char *additional_model_paths,
        char *log_path, enum VmafLogFmt log_fmt, bool disable_clip, bool disable_avx, bool enable_transform,
        int vmaf_feature_setting, enum VmafPoolingMethod pool_method, int n_thread, int n_subsample, bool enable_conf_interval)
{
    double score;

    int ret = 0;
    struct data *s;
    s = (struct data *)malloc(sizeof(struct data));
    s->format = pix_fmt;
    s->use_color = vmaf_feature_setting & VMAF_FEATURE_SETTING_DO_COLOR;
    s->width = width;
    s->height = height;
    s->ref_rfile = NULL;
    s->dis_rfile = NULL;

    if (pix_fmt == VMAF_PIX_FMT_YUV420P || pix_fmt == VMAF_PIX_FMT_YUV420P10LE)
    {
        if ((width * height) % 2 != 0)
        {
            fprintf(stderr, "(width * height) %% 2 != 0, width = %d, height = %d.\n", width, height);
            ret = 1;
            goto fail_or_end;
        }
        s->offset = width * height / 2;
    }
    else if (pix_fmt == VMAF_PIX_FMT_YUV422P || pix_fmt == VMAF_PIX_FMT_YUV422P10LE)
    {
        s->offset = width * height;
    }
    else if (pix_fmt == VMAF_PIX_FMT_YUV444P || pix_fmt == VMAF_PIX_FMT_YUV444P10LE)
    {
        s->offset = width * height * 2;
    }
    else
    {
        fprintf(stderr, "Unknown format.\n");
        ret = 1;
        goto fail_or_end;
    }

    if (!(s->ref_rfile = fopen(ref_path, "rb")))
    {
        fprintf(stderr, "fopen ref_path %s failed.\n", ref_path);
        ret = 1;
        goto fail_or_end;
    }


    if (!(s->dis_rfile = fopen(dis_path, "rb")))
    {
        fprintf(stderr, "fopen dis_path %s failed.\n", dis_path);
        ret = 1;
        goto fail_or_end;
    }

    if (strcmp(ref_path, "-"))
    {
#ifdef _WIN32
        struct _stat64 ref_stat;
        if (!_stat64(ref_path, &ref_stat))
#else
        struct stat64 ref_stat;
        if (!stat64(ref_path, &ref_stat))
#endif
        {
            size_t frame_size = width * height + s->offset;
            if (pix_fmt == VMAF_PIX_FMT_YUV420P10LE || pix_fmt == VMAF_PIX_FMT_YUV422P10LE || pix_fmt == VMAF_PIX_FMT_YUV444P10LE)
            {
                frame_size *= 2;
            }

            s->num_frames = ref_stat.st_size / frame_size;
        }
        else
        {
            s->num_frames = -1;
        }
    }
    else
    {
        s->num_frames = -1;
    }

    // initialize context
    VmafSettings *vmafSettings;
    vmafSettings = (VmafSettings *)malloc(sizeof(VmafSettings));

    // fill context with data
    vmafSettings->pix_fmt = pix_fmt;

    vmafSettings->width = width;
    vmafSettings->height = height;

    vmafSettings->log_path = log_path;

    vmafSettings->model_path = model_path;
    vmafSettings->additional_model_paths = additional_model_paths;
    vmafSettings->disable_clip = disable_clip;
    vmafSettings->disable_avx = disable_avx;
    vmafSettings->enable_transform = enable_transform;
    vmafSettings->enable_conf_interval = enable_conf_interval;

    vmafSettings->log_fmt = log_fmt;
    vmafSettings->vmaf_feature_setting = vmaf_feature_setting;
    vmafSettings->pool_method = pool_method;

    vmafSettings->n_thread = n_thread;
    vmafSettings->n_subsample = n_subsample;

    /* Run VMAF */
    ret = compute_vmaf(&score, read_frame, read_vmaf_picture, s, vmafSettings);

    // free VMAF context
    free(vmafSettings);

fail_or_end:
    if (s->ref_rfile)
    {
        fclose(s->ref_rfile);
    }
    if (s->dis_rfile)
    {
        fclose(s->dis_rfile);
    }
    if (s)
    {
        free(s);
    }
    return ret;
}

int main(int argc, char *argv[])
{
    int width;
    int height;
    char* ref_path;
    char* dis_path;
    char *model_path;
    char *additional_model_paths;
    char *log_path = NULL;

    enum VmafPixelFormat pix_fmt = VMAF_PIX_FMT_UNKNOWN;
    enum VmafLogFmt log_fmt = VMAF_LOG_FMT_XML;
    enum VmafPoolingMethod pool_method = VMAF_POOL_MEAN;

    bool disable_clip = false;
    bool disable_avx = false;
    bool enable_transform = false;

    int n_thread = 0;
    int n_subsample = 1;
    bool enable_conf_interval = false;
    char *temp;
#if MEM_LEAK_TEST_ENABLE	
	int itr_ctr;
	int ret = 0;
#endif
    /* Check parameters */

    if (argc < 7)
    {
        print_usage(argc, argv);
        return -1;
    }

    char *pix_fmt_option = argv[1];

    if (!strcmp(pix_fmt_option, "yuv420p"))
    {
        pix_fmt = VMAF_PIX_FMT_YUV420P;
    }
    else if (!strcmp(pix_fmt_option, "yuv422p"))
    {
        pix_fmt = VMAF_PIX_FMT_YUV422P;
    }
        else if (!strcmp(pix_fmt_option, "yuv444p"))
    {
        pix_fmt = VMAF_PIX_FMT_YUV444P;
    }
        else if (!strcmp(pix_fmt_option, "yuv420p10le"))
    {
        pix_fmt = VMAF_PIX_FMT_YUV420P10LE;
    }
        else if (!strcmp(pix_fmt_option, "yuv422p10le"))
    {
        pix_fmt = VMAF_PIX_FMT_YUV422P10LE;
    }
        else if (!strcmp(pix_fmt_option, "yuv444p10le"))
    {
        pix_fmt = VMAF_PIX_FMT_YUV444P10LE;
    }
    else
    {
        fprintf(stderr, "Unknown format %s.\n", pix_fmt_option);
        print_usage(argc, argv);
        return -1;
    }

    try
    {
        width = std::stoi(argv[2]);
        height = std::stoi(argv[3]);
    }
    catch (std::logic_error& e)
    {
        fprintf(stderr, "Error: Invalid width/height format: %s\n", e.what());
        print_usage(argc, argv);
        return -1;
    }

    if (width <= 0 || height <= 0)
    {
        fprintf(stderr, "Error: Invalid width/height value: %d, %d\n", width, height);
        print_usage(argc, argv);
        return -1;
    }

    ref_path = argv[4];
    dis_path = argv[5];
    model_path = argv[6];

    log_path = getCmdOption(argv + 7, argv + argc, "--log");

    char *log_fmt_option = getCmdOption(argv + 7, argv + argc, "--log-fmt");
    if (log_fmt_option != NULL && !(strcmp(log_fmt_option, "xml") == 0 || strcmp(log_fmt_option, "json") == 0 || strcmp(log_fmt_option, "csv") == 0))
    {
        fprintf(stderr, "Error: log_fmt must be xml, json or csv, but is %s\n", log_fmt_option);
        return -1;
    }

    if ((log_fmt_option != NULL) && (strcmp(log_fmt_option, "json") == 0))
    {
        log_fmt = VMAF_LOG_FMT_JSON;
    }
    else if ((log_fmt_option != NULL) && (strcmp(log_fmt_option, "csv") == 0))
    {
        log_fmt = VMAF_LOG_FMT_CSV;
    }

    temp = getCmdOption(argv + 7, argv + argc, "--thread");
    if (temp)
    {
        try
        {
            n_thread = std::stoi(temp);
        }
        catch (std::logic_error& e)
        {
            fprintf(stderr, "Error: Invalid n_thread format: %s\n", e.what());
            print_usage(argc, argv);
            return -1;
        }
    }
    if (n_thread < 0)
    {
        fprintf(stderr, "Error: Invalid n_thread value: %d\n", n_thread);
        print_usage(argc, argv);
        return -1;
    }

    temp = getCmdOption(argv + 7, argv + argc, "--subsample");
    if (temp)
    {
        try
        {
            n_subsample = std::stoi(temp);
        }
        catch (std::logic_error& e)
        {
            fprintf(stderr, "Error: Invalid n_subsample format: %s\n", e.what());
            print_usage(argc, argv);
            return -1;
        }
    }
    if (n_subsample <= 0)
    {
        fprintf(stderr, "Error: Invalid n_subsample value: %d\n", n_subsample);
        print_usage(argc, argv);
        return -1;
    }

    if (cmdOptionExists(argv + 7, argv + argc, "--disable-clip"))
    {
        disable_clip = true;
    }

    if (cmdOptionExists(argv + 7, argv + argc, "--disable-avx"))
    {
        disable_avx = true;
    }

    if (cmdOptionExists(argv + 7, argv + argc, "--enable-transform"))
    {
        enable_transform = true;
    }

    // populate VmafFeatureSetting

    int vmaf_feature_setting = VMAF_FEATURE_SETTING_DO_NONE;

    if (cmdOptionExists(argv + 7, argv + argc, "--psnr"))
    {
        vmaf_feature_setting |= VMAF_FEATURE_SETTING_DO_PSNR;
    }

    if (cmdOptionExists(argv + 7, argv + argc, "--ssim"))
    {
        vmaf_feature_setting |= VMAF_FEATURE_SETTING_DO_SSIM;
    }

    if (cmdOptionExists(argv + 7, argv + argc, "--ms-ssim"))
    {
        vmaf_feature_setting |= VMAF_FEATURE_SETTING_DO_MS_SSIM;
    }

    if (cmdOptionExists(argv + 7, argv + argc, "--color"))
    {
        vmaf_feature_setting |= VMAF_FEATURE_SETTING_DO_COLOR;
    }

    char *pool_method_option = getCmdOption(argv + 7, argv + argc, "--pool");
    if (pool_method_option != NULL && !(strcmp(pool_method_option, "min") == 0 || strcmp(pool_method_option, "harmonic_mean") == 0 || strcmp(pool_method_option, "mean") == 0))
    {
        fprintf(stderr, "Error: pool_method must be min, harmonic_mean or mean, but is %s\n", pool_method_option);
        return -1;
    }

    if ((pool_method_option != NULL) && (strcmp(pool_method_option, "min") == 0))
    {
        pool_method = VMAF_POOL_MIN;
    }
    else if ((pool_method_option != NULL) && (strcmp(pool_method_option, "harmonic_mean") == 0))
    {
        pool_method = VMAF_POOL_HARMONIC_MEAN;
    }

    additional_model_paths = getCmdOption(argv + 7, argv + argc, "--additional-models");

    if (cmdOptionExists(argv + 7, argv + argc, "--ci"))
    {
        enable_conf_interval = true;
    }

    try
    {
#if MEM_LEAK_TEST_ENABLE
		for(itr_ctr = 0; itr_ctr < 1000; itr_ctr ++)
		{
			getMemory(itr_ctr, 1);
			ret = run_wrapper(pix_fmt, width, height, ref_path, dis_path, model_path, additional_model_paths,
                log_path, log_fmt, disable_clip, disable_avx, enable_transform,
                vmaf_feature_setting, pool_method, n_thread, n_subsample, enable_conf_interval);
			getMemory(itr_ctr, 2);
		}
#else
        return run_wrapper(pix_fmt, width, height, ref_path, dis_path, model_path, additional_model_paths,
                log_path, log_fmt, disable_clip, disable_avx, enable_transform,
                vmaf_feature_setting, pool_method, n_thread, n_subsample, enable_conf_interval);
#endif
    }
    catch (const std::exception &e)
    {
        fprintf(stderr, "Error: %s\n", e.what());
        print_usage(argc, argv);
        return -1;
    }
    
}
