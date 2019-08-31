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

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "alloc.h"
#include "file_io.h"
#include "frame.h"

#define read_image_b       read_image_b2s
#define read_image_w       read_image_w2s

static int completed_frames = 0;

int read_vmaf_picture(VmafPicture *ref_vmaf_pict, VmafPicture *dis_vmaf_pict, float *temp_data, int stride_byte, void *s)
{
    struct data *user_data = (struct data *)s;
    enum VmafPixelFormat fmt = user_data->format;
    int w = user_data->width;
    int h = user_data->height;
    bool use_color = user_data->use_color;
    int ret;

    // read ref y
    if (fmt == VMAF_PIX_FMT_YUV420P || fmt == VMAF_PIX_FMT_YUV422P || fmt == VMAF_PIX_FMT_YUV444P)
    {
        ret = read_image_b(user_data->ref_rfile, ref_vmaf_pict->data_y, 0, w, h, stride_byte);
    }
    else if (fmt == VMAF_PIX_FMT_YUV420P10LE || fmt == VMAF_PIX_FMT_YUV422P10LE || fmt == VMAF_PIX_FMT_YUV444P10LE)
    {
        ret = read_image_w(user_data->ref_rfile, ref_vmaf_pict->data_y, 0, w, h, stride_byte);
    }
    else
    {
        fprintf(stderr, "unknown format %s.\n", fmt);
        return 1;
    }
    if (ret)
    {
        if (feof(user_data->ref_rfile))
        {
            ret = 2; // OK if end of file
        }
        return ret;
    }

    // read dis y
    if (fmt == VMAF_PIX_FMT_YUV420P || fmt == VMAF_PIX_FMT_YUV422P || fmt == VMAF_PIX_FMT_YUV444P)
    {
        ret = read_image_b(user_data->dis_rfile, dis_vmaf_pict->data_y, 0, w, h, stride_byte);
    }
    else if (fmt == VMAF_PIX_FMT_YUV420P10LE || fmt == VMAF_PIX_FMT_YUV422P10LE || fmt == VMAF_PIX_FMT_YUV444P10LE)
    {
        ret = read_image_w(user_data->dis_rfile, dis_vmaf_pict->data_y, 0, w, h, stride_byte);
    }
    else
    {
        fprintf(stderr, "unknown format %s.\n", fmt);
        return 1;
    }
    if (ret)
    {
        if (feof(user_data->dis_rfile))
        {
            ret = 2; // OK if end of file
        }
        return ret;
    }

    if (!use_color)
    {

        // ref skip u and v
        if (fmt == VMAF_PIX_FMT_YUV420P || fmt == VMAF_PIX_FMT_YUV422P || fmt == VMAF_PIX_FMT_YUV444P)
        {
            if (fread(temp_data, 1, user_data->offset, user_data->ref_rfile) != (size_t)user_data->offset)
            {
                fprintf(stderr, "ref fread u and v failed.\n");
                goto fail_or_end;
            }
        }
        else if (fmt == VMAF_PIX_FMT_YUV420P10LE || fmt == VMAF_PIX_FMT_YUV422P10LE || fmt == VMAF_PIX_FMT_YUV444P10LE)
        {
            if (fread(temp_data, 2, user_data->offset, user_data->ref_rfile) != (size_t)user_data->offset)
            {
                fprintf(stderr, "ref fread u and v failed.\n");
                goto fail_or_end;
            }
        }
        else
        {
            fprintf(stderr, "unknown format %s.\n", fmt);
            goto fail_or_end;
        }

        // dis skip u and v
        if (fmt == VMAF_PIX_FMT_YUV420P || fmt == VMAF_PIX_FMT_YUV422P || fmt == VMAF_PIX_FMT_YUV444P)
        {
            if (fread(temp_data, 1, user_data->offset, user_data->dis_rfile) != (size_t)user_data->offset)
            {
                fprintf(stderr, "dis fread u and v failed.\n");
                goto fail_or_end;
            }
        }
        else if (fmt == VMAF_PIX_FMT_YUV420P10LE || fmt == VMAF_PIX_FMT_YUV422P10LE || fmt == VMAF_PIX_FMT_YUV444P10LE)
        {
            if (fread(temp_data, 2, user_data->offset, user_data->dis_rfile) != (size_t)user_data->offset)
            {
                fprintf(stderr, "dis fread u and v failed.\n");
                goto fail_or_end;
            }
        }
        else
        {
            fprintf(stderr, "unknown format %s.\n", fmt);
            goto fail_or_end;
        }
    }
    else
    {

        size_t w_u = 0, w_v = 0, h_u = 0, h_v = 0;

        int color_resolution_ret = get_color_resolution(fmt, w, h, &w_u, &h_u, &w_v, &h_v);

        if (color_resolution_ret) {
            fprintf(stderr, "Calculating resolutions for color channels failed B.\n");
            goto fail_or_end;
        }

        size_t stride_byte_u = get_stride_byte_from_width(w_u);
        size_t stride_byte_v = get_stride_byte_from_width(w_v);

        // read ref u
        if (fmt == VMAF_PIX_FMT_YUV420P || fmt == VMAF_PIX_FMT_YUV422P || fmt == VMAF_PIX_FMT_YUV444P)
        {
            ret = read_image_b(user_data->ref_rfile, ref_vmaf_pict->data_u, 0, w_u, h_u, stride_byte_u);
        }
        else if (fmt == VMAF_PIX_FMT_YUV420P10LE || fmt == VMAF_PIX_FMT_YUV422P10LE || fmt == VMAF_PIX_FMT_YUV444P10LE)
        {
            ret = read_image_w(user_data->ref_rfile, ref_vmaf_pict->data_u, 0, w_u, h_u, stride_byte_u);
        }
        else
        {
            fprintf(stderr, "unknown format %s.\n", fmt);
            return 1;
        }
        if (ret)
        {
            if (feof(user_data->ref_rfile))
            {
                ret = 2; // OK if end of file
            }
            return ret;
        }

        // read dis u
        if (fmt == VMAF_PIX_FMT_YUV420P || fmt == VMAF_PIX_FMT_YUV422P || fmt == VMAF_PIX_FMT_YUV444P)
        {
            ret = read_image_b(user_data->dis_rfile, dis_vmaf_pict->data_u, 0, w_u, h_u, stride_byte_u);
        }
        else if (fmt == VMAF_PIX_FMT_YUV420P10LE || fmt == VMAF_PIX_FMT_YUV422P10LE || fmt == VMAF_PIX_FMT_YUV444P10LE)
        {
            ret = read_image_w(user_data->dis_rfile, dis_vmaf_pict->data_u, 0, w_u, h_u, stride_byte_u);
        }
        else
        {
            fprintf(stderr, "unknown format %s.\n", fmt);
            return 1;
        }
        if (ret)
        {
            if (feof(user_data->dis_rfile))
            {
                ret = 2; // OK if end of file
            }
            return ret;
        }

        // read ref v
        if (fmt == VMAF_PIX_FMT_YUV420P || fmt == VMAF_PIX_FMT_YUV422P || fmt == VMAF_PIX_FMT_YUV444P)
        {
            ret = read_image_b(user_data->ref_rfile, ref_vmaf_pict->data_v, 0, w_v, h_v, stride_byte_v);
        }
        else if (fmt == VMAF_PIX_FMT_YUV420P10LE || fmt == VMAF_PIX_FMT_YUV422P10LE || fmt == VMAF_PIX_FMT_YUV444P10LE)
        {
            ret = read_image_w(user_data->ref_rfile, ref_vmaf_pict->data_v, 0, w_v, h_v, stride_byte_v);
        }
        else
        {
            fprintf(stderr, "unknown format %s.\n", fmt);
            return 1;
        }
        if (ret)
        {
            if (feof(user_data->ref_rfile))
            {
                ret = 2; // OK if end of file
            }
            return ret;
        }

        // read dis v
        if (fmt == VMAF_PIX_FMT_YUV420P || fmt == VMAF_PIX_FMT_YUV422P || fmt == VMAF_PIX_FMT_YUV444P)
        {
            ret = read_image_b(user_data->dis_rfile, dis_vmaf_pict->data_v, 0, w_v, h_v, stride_byte_v);
        }
        else if (fmt == VMAF_PIX_FMT_YUV420P10LE || fmt == VMAF_PIX_FMT_YUV422P10LE || fmt == VMAF_PIX_FMT_YUV444P10LE)
        {
            ret = read_image_w(user_data->dis_rfile, dis_vmaf_pict->data_v, 0, w_v, h_v, stride_byte_v);
        }
        else
        {
            fprintf(stderr, "unknown format %s.\n", fmt);
            return 1;
        }
        if (ret)
        {
            if (feof(user_data->dis_rfile))
            {
                ret = 2; // OK if end of file
            }
            return ret;
        }
    }

fail_or_end:
    return ret;
}

int read_frame(float *ref_data, float *dis_data, float *temp_data, int stride_byte, void *s)
{
    struct data *user_data = (struct data *)s;
    enum VmafPixelFormat fmt = user_data->format;
    int w = user_data->width;
    int h = user_data->height;
    int ret;

    // read ref y
    if (fmt == VMAF_PIX_FMT_YUV420P || fmt == VMAF_PIX_FMT_YUV422P || fmt == VMAF_PIX_FMT_YUV444P)
    {
        ret = read_image_b(user_data->ref_rfile, ref_data, 0, w, h, stride_byte);
    }
    else if (fmt == VMAF_PIX_FMT_YUV420P10LE || fmt == VMAF_PIX_FMT_YUV422P10LE || fmt == VMAF_PIX_FMT_YUV444P10LE)
    {
        ret = read_image_w(user_data->ref_rfile, ref_data, 0, w, h, stride_byte);
    }
    else
    {
        fprintf(stderr, "unknown format %s.\n", fmt);
        return 1;
    }
    if (ret)
    {
        if (feof(user_data->ref_rfile))
        {
            ret = 2; // OK if end of file
        }
        return ret;
    }

    // read dis y
    if (fmt == VMAF_PIX_FMT_YUV420P || fmt == VMAF_PIX_FMT_YUV422P || fmt == VMAF_PIX_FMT_YUV444P)
    {
        ret = read_image_b(user_data->dis_rfile, dis_data, 0, w, h, stride_byte);
    }
    else if (fmt == VMAF_PIX_FMT_YUV420P10LE || fmt == VMAF_PIX_FMT_YUV422P10LE || fmt == VMAF_PIX_FMT_YUV444P10LE)
    {
        ret = read_image_w(user_data->dis_rfile, dis_data, 0, w, h, stride_byte);
    }
    else
    {
        fprintf(stderr, "unknown format %s.\n", fmt);
        return 1;
    }
    if (ret)
    {
        if (feof(user_data->dis_rfile))
        {
            ret = 2; // OK if end of file
        }
        return ret;
    }

    // ref skip u and v
    if (fmt == VMAF_PIX_FMT_YUV420P || fmt == VMAF_PIX_FMT_YUV422P || fmt == VMAF_PIX_FMT_YUV444P)
    {
        if (fread(temp_data, 1, user_data->offset, user_data->ref_rfile) != (size_t)user_data->offset)
        {
            fprintf(stderr, "ref fread u and v failed.\n");
            goto fail_or_end;
        }
    }
    else if (fmt == VMAF_PIX_FMT_YUV420P10LE || fmt == VMAF_PIX_FMT_YUV422P10LE || fmt == VMAF_PIX_FMT_YUV444P10LE)
    {
        if (fread(temp_data, 2, user_data->offset, user_data->ref_rfile) != (size_t)user_data->offset)
        {
            fprintf(stderr, "ref fread u and v failed.\n");
            goto fail_or_end;
        }
    }
    else
    {
        fprintf(stderr, "unknown format %s.\n", fmt);
        goto fail_or_end;
    }

    // dis skip u and v
    if (fmt == VMAF_PIX_FMT_YUV420P || fmt == VMAF_PIX_FMT_YUV422P || fmt == VMAF_PIX_FMT_YUV444P)
    {
        if (fread(temp_data, 1, user_data->offset, user_data->dis_rfile) != (size_t)user_data->offset)
        {
            fprintf(stderr, "dis fread u and v failed.\n");
            goto fail_or_end;
        }
    }
    else if (fmt == VMAF_PIX_FMT_YUV420P10LE || fmt == VMAF_PIX_FMT_YUV422P10LE || fmt == VMAF_PIX_FMT_YUV444P10LE)
    {
        if (fread(temp_data, 2, user_data->offset, user_data->dis_rfile) != (size_t)user_data->offset)
        {
            fprintf(stderr, "dis fread u and v failed.\n");
            goto fail_or_end;
        }
    }
    else
    {
        fprintf(stderr, "unknown format %s.\n", fmt);
        goto fail_or_end;
    }
    
    fprintf(stderr, "Frame: %d/%d\r", completed_frames++, user_data->num_frames);

fail_or_end:
    return ret;
}

int read_noref_frame(float *dis_data, float *temp_data, int stride_byte, void *s)
{
    struct noref_data *user_data = (struct noref_data *)s;
    enum VmafPixelFormat fmt = user_data->format;
    int w = user_data->width;
    int h = user_data->height;
    int ret;

    // read dis y
    if (fmt == VMAF_PIX_FMT_YUV420P || fmt == VMAF_PIX_FMT_YUV422P || fmt == VMAF_PIX_FMT_YUV444P)
    {
        ret = read_image_b(user_data->dis_rfile, dis_data, 0, w, h, stride_byte);
    }
    else if (fmt == VMAF_PIX_FMT_YUV420P10LE || fmt == VMAF_PIX_FMT_YUV422P10LE || fmt == VMAF_PIX_FMT_YUV444P10LE)
    {
        ret = read_image_w(user_data->dis_rfile, dis_data, 0, w, h, stride_byte);
    }
    else
    {
        fprintf(stderr, "unknown format %s.\n", fmt);
        return 1;
    }
    if (ret)
    {
        if (feof(user_data->dis_rfile))
        {
            ret = 2; // OK if end of file
        }
        return ret;
    }

    // dis skip u and v
    if (fmt == VMAF_PIX_FMT_YUV420P || fmt == VMAF_PIX_FMT_YUV422P || fmt == VMAF_PIX_FMT_YUV444P)
    {
        if (fread(temp_data, 1, user_data->offset, user_data->dis_rfile) != (size_t)user_data->offset)
        {
            fprintf(stderr, "dis fread u and v failed.\n");
            goto fail_or_end;
        }
    }
    else if (fmt == VMAF_PIX_FMT_YUV420P10LE || fmt == VMAF_PIX_FMT_YUV422P10LE || fmt == VMAF_PIX_FMT_YUV444P10LE)
    {
        if (fread(temp_data, 2, user_data->offset, user_data->dis_rfile) != (size_t)user_data->offset)
        {
            fprintf(stderr, "dis fread u and v failed.\n");
            goto fail_or_end;
        }
    }
    else
    {
        fprintf(stderr, "unknown format %s.\n", fmt);
        goto fail_or_end;
    }


fail_or_end:
    return ret;
}

int get_frame_offset(enum VmafPixelFormat fmt, int w, int h, size_t *offset)
{
    if (fmt == VMAF_PIX_FMT_YUV420P || fmt == VMAF_PIX_FMT_YUV420P10LE)
    {
        if ((w * h) % 2 != 0)
        {
            fprintf(stderr, "(width * height) %% 2 != 0, width = %d, height = %d.\n", w, h);
            return 1;
        }
        *offset = w * h / 2;
    }
    else if (fmt == VMAF_PIX_FMT_YUV422P || fmt == VMAF_PIX_FMT_YUV422P10LE)
    {
        *offset = w * h;
    }
    else if (fmt == VMAF_PIX_FMT_YUV444P || fmt == VMAF_PIX_FMT_YUV444P10LE)
    {
        *offset = w * h * 2;
    }
    else
    {
        fprintf(stderr, "unknown format %s.\n", fmt);
        return 1;
    }
    return 0;
}

int get_color_resolution(enum VmafPixelFormat fmt, int w, int h, size_t *w_u, size_t *h_u, size_t *w_v, size_t *h_v) {

    // check that both width and height are positive
    if ((w <= 0) || (h <= 0))
    {
        fprintf(stderr, "Width or height is not positive, width = %d, height = %d.\n", w, h);
        return 1;
    }

    if (fmt == VMAF_PIX_FMT_YUV420P || fmt == VMAF_PIX_FMT_YUV420P10LE)
    {
        // check that both width and height are even, i.e., their product is divisible by 2
        if ((w * h) % 2 != 0)
        {
            fprintf(stderr, "(width * height) %% 2 != 0, width = %d, height = %d.\n", w, h);
            return 1;
        }
        *w_u = w / 2;
        *w_v = w / 2;
        *h_u = h / 2;
        *h_v = h / 2;
    }
    else if (fmt == VMAF_PIX_FMT_YUV422P || fmt == VMAF_PIX_FMT_YUV422P10LE)
    {
        // need to double check this
        *w_u = w / 2;
        *w_v = w / 2;
        *h_u = h;
        *h_v = h;
    }
    else if (fmt == VMAF_PIX_FMT_YUV444P || fmt == VMAF_PIX_FMT_YUV444P10LE)
    {
        *w_u = w;
        *w_v = w;
        *h_u = h;
        *h_v = h;
    }
    else
    {
        fprintf(stderr, "unknown format %s.\n", fmt);
        return 1;
    }

    if (*w_u <= 0 || *h_u <= 0 || (size_t)*w_u > ALIGN_FLOOR(INT_MAX) / sizeof(float))
    {
        fprintf(stderr, "Invalid width and height for u, width = %d, height = %d.\n", *w_u, *h_u);
        return 1;
    }

    if (*w_v <= 0 || *h_v <= 0 || (size_t)*w_v > ALIGN_FLOOR(INT_MAX) / sizeof(float))
    {
        fprintf(stderr, "Invalid width and height for v, width = %d, height = %d.\n", *w_v, *h_v);
        return 1;
    }

    return 0;
}

int get_stride_byte_from_width(int w) {
    return ALIGN_CEIL(w * sizeof(float));
}
