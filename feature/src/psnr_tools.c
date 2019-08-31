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

#include <stdio.h>
#include <string.h>
#include "psnr_tools.h"

/*
 * For a given format, returns the constants to use in psnr based calculations
 */
int psnr_constants(enum VmafPixelFormat fmt, double *peak, double *psnr_max) {
    if (fmt == VMAF_PIX_FMT_YUV420P || fmt == VMAF_PIX_FMT_YUV422P || fmt == VMAF_PIX_FMT_YUV444P)
    {
        // max psnr 60.0 for 8-bit per Ioannis
        *peak = 255.0;
        *psnr_max = 60.0;
    }
    else if (fmt == VMAF_PIX_FMT_YUV420P10LE || fmt == VMAF_PIX_FMT_YUV422P10LE || fmt == VMAF_PIX_FMT_YUV444P10LE)
    {
        // 10 bit gets normalized to 8 bit, peak is 1023 / 4.0 = 255.75
        // max psnr 72.0 for 10-bit per Ioannis
        *peak = 255.75;
        *psnr_max = 72.0;
    }
    else
    {
        return 1;
    }

    return 0;
}
