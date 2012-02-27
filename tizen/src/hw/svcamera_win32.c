/*
 * Virtual Camera device(PCI) for Windows host.
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Contact:
 * JinHyung Jo <jinhyung.jo@samsung.com>
 * YeongKyoon Lee <yeongkyoon.lee@samsung.com>
 * DongKyun Yun <dk77.yun@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Contributors:
 * - S-Core Co., Ltd
 *
 */


#include "qemu-common.h"
#include "svcamera.h"
#include "pci.h"
#include "kvm.h"
#include "tizen/src/debug_ch.h"

#include "windows.h"
#include "basetyps.h"
#include "mmsystem.h"

MULTI_DEBUG_CHANNEL(tizen, camera_win32);

// V4L2 defines copy from videodev2.h
#define V4L2_CTRL_FLAG_SLIDER       0x0020

#define V4L2_CTRL_CLASS_USER        0x00980000
#define V4L2_CID_BASE               (V4L2_CTRL_CLASS_USER | 0x900)
#define V4L2_CID_BRIGHTNESS         (V4L2_CID_BASE+0)
#define V4L2_CID_CONTRAST           (V4L2_CID_BASE+1)
#define V4L2_CID_SATURATION         (V4L2_CID_BASE+2)
#define V4L2_CID_SHARPNESS          (V4L2_CID_BASE+27)

#define V4L2_PIX_FMT_YUYV    MAKEFOURCC('Y', 'U', 'Y', 'V') /* 16  YUV 4:2:2     */
#define V4L2_PIX_FMT_YUV420  MAKEFOURCC('Y', 'U', '1', '2') /* 12  YUV 4:2:0     */
#define V4L2_PIX_FMT_YVU420  MAKEFOURCC('Y', 'V', '1', '2') /* 12  YVU 4:2:0     */

enum {
    HWC_OPEN,
    HWC_CLOSE,
    HWC_START,
    HWC_STOP,
    HWC_S_FPS,
    HWC_G_FPS,
    HWC_S_FMT,
    HWC_G_FMT,
    HWC_TRY_FMT,
    HWC_ENUM_FMT,
    HWC_QCTRL,
    HWC_S_CTRL,
    HWC_G_CTRL,
    HWC_ENUM_FSIZES,
    HWC_ENUM_INTERVALS
};

typedef enum tagVideoProcAmpProperty {
  VideoProcAmp_Brightness,
  VideoProcAmp_Contrast,
  VideoProcAmp_Hue,
  VideoProcAmp_Saturation,
  VideoProcAmp_Sharpness,
  VideoProcAmp_Gamma,
  VideoProcAmp_ColorEnable,
  VideoProcAmp_WhiteBalance,
  VideoProcAmp_BacklightCompensation,
  VideoProcAmp_Gain
} VideoProcAmpProperty;

typedef struct tagHWCParam {
    long val1;
    long val2;
    long val3;
    long val4;
    long val5;
} HWCParam;

typedef struct tagSVCamConvertPixfmt {
    uint32_t fmt;   /* fourcc */
    uint32_t bpp;   /* bits per pixel, 0 for compressed formats */
    uint32_t needs_conversion;
} SVCamConvertPixfmt;

static SVCamConvertPixfmt supported_dst_pixfmts[] = {
        { V4L2_PIX_FMT_YUYV, 16, 0 },
        { V4L2_PIX_FMT_YUV420, 12, 0 },
        { V4L2_PIX_FMT_YVU420, 12, 0 },
};

typedef struct tagSVCamConvertFrameInfo {
    uint32_t width;
    uint32_t height;
} SVCamConvertFrameInfo;

static SVCamConvertFrameInfo supported_dst_frames[] = {
        { 640, 480 },
        { 352, 288 },
        { 320, 240 },
        { 176, 144 },
        { 160, 120 },
};

#define SVCAM_CTRL_VALUE_MAX        20
#define SVCAM_CTRL_VALUE_MIN        1
#define SVCAM_CTRL_VALUE_MID        10
#define SVCAM_CTRL_VALUE_STEP       1

struct svcam_qctrl {
    uint32_t id;
    uint32_t hit;
    long min;
    long max;
    long step;
    long init_val;
};

static struct svcam_qctrl qctrl_tbl[] = {
    { V4L2_CID_BRIGHTNESS, 0, },
    { V4L2_CID_CONTRAST, 0, },
    { V4L2_CID_SATURATION,0, },
    { V4L2_CID_SHARPNESS, 0, },
};

typedef int (STDAPICALLTYPE *CallbackFn)(ULONG dwSize, BYTE *pBuffer);
typedef HRESULT (STDAPICALLTYPE *CTRLFN)(UINT, UINT, LPVOID);
typedef HRESULT (STDAPICALLTYPE *SETCALLBACKFN)(CallbackFn);


static HINSTANCE g_hInst = NULL;
static SVCamState *g_state = NULL;

static CTRLFN SVCamCtrl;
static SETCALLBACKFN SVCamSetCallbackFn;

static uint32_t cur_fmt_idx = 0;
static uint32_t cur_frame_idx = 0;

void v4lconvert_yuyv_to_yuv420(const unsigned char *src, unsigned char *dest,
        uint32_t width, uint32_t height, uint32_t yvu);


static long value_convert_from_guest(long min, long max, long value)
{
    double rate = 0.0;
    long dist = 0, ret = 0;

    dist = max - min;

    if (dist < SVCAM_CTRL_VALUE_MAX) {
        rate = (double)SVCAM_CTRL_VALUE_MAX / (double)dist;
        ret = min + (int32_t)(value / rate);
    } else {
        rate = (double)dist / (double)SVCAM_CTRL_VALUE_MAX;
        ret = min + (int32_t)(rate * value);
    }
    return ret;
}

static long value_convert_to_guest(long min, long max, long value)
{
    double rate  = 0.0;
    long dist = 0, ret = 0;

    dist = max - min;

    if (dist < SVCAM_CTRL_VALUE_MAX) {
        rate = (double)SVCAM_CTRL_VALUE_MAX / (double)dist;
        ret = (int32_t)((double)(value - min) * rate);
    } else {
        rate = (double)dist / (double)SVCAM_CTRL_VALUE_MAX;
        ret = (int32_t)((double)(value - min) / rate);
    }

    return ret;
}

static int STDAPICALLTYPE svcam_device_callbackfn(ULONG dwSize, BYTE *pBuffer)
{
    static uint32_t index = 0;
    uint32_t width, height;
    width = supported_dst_frames[cur_frame_idx].width;
    height = supported_dst_frames[cur_frame_idx].height;
    void *buf = g_state->vaddr + (g_state->buf_size * index);

    switch (supported_dst_pixfmts[cur_fmt_idx].fmt) {
    case V4L2_PIX_FMT_YUV420:
        v4lconvert_yuyv_to_yuv420(pBuffer, buf, width, height, 0);
        break;
    case V4L2_PIX_FMT_YVU420:
        v4lconvert_yuyv_to_yuv420(pBuffer, buf, width, height, 1);
        break;
    case V4L2_PIX_FMT_YUYV:
        memcpy(buf, (void*)pBuffer, dwSize);
        break;
    }
    index = !index;

    if (g_state->req_frame) {
        qemu_irq_raise(g_state->dev.irq[2]);
        g_state->req_frame = 0;
    }
    return 1;
}

// SVCAM_CMD_INIT
void svcam_device_init(SVCamState* state)
{
    SVCamThreadInfo *thread = state->thread;

    pthread_cond_init(&thread->thread_cond, NULL);
    pthread_mutex_init(&thread->mutex_lock, NULL);

    g_state = state;
}

// SVCAM_CMD_OPEN
void svcam_device_open(SVCamState* state)
{
    HRESULT hr;
    SVCamParam *param = state->thread->param;
    param->top = 0;

    g_hInst = LoadLibrary("hwcfilter.dll");

    if (!g_hInst) {
        g_hInst = LoadLibrary("bin\\hwcfilter.dll");
        if (!g_hInst) {
            ERR("load library failed!!!!\n");
            param->errCode = EINVAL;
            return;
        }
    }

    SVCamCtrl = (CTRLFN)GetProcAddress(g_hInst, "HWCCtrl");
    if (!SVCamCtrl) {
        ERR("HWCCtrl get failed!!!\n");
        FreeLibrary(g_hInst);
        param->errCode = EINVAL;
        return;
    }

    SVCamSetCallbackFn = (SETCALLBACKFN)GetProcAddress(g_hInst, "HWCSetCallback");
    if (!SVCamSetCallbackFn) {
        ERR("HWCSetCallback get failed!!!\n");
        FreeLibrary(g_hInst);
        param->errCode = EINVAL;
        return;
    }

    hr = SVCamCtrl(HWC_OPEN, 0, NULL);
    if (FAILED(hr)) {
        param->errCode = EINVAL;
        FreeLibrary(g_hInst);
        ERR("camera device open failed!!!, [HRESULT : 0x%x]\n", hr);
        return;
    }
    hr = SVCamSetCallbackFn((CallbackFn)svcam_device_callbackfn);
    if (FAILED(hr)) {
        param->errCode = EINVAL;
        SVCamCtrl(HWC_CLOSE, 0, NULL);
        FreeLibrary(g_hInst);
        ERR("call back function set failed!!!, [HRESULT : 0x%x]\n", hr);
    }

    TRACE("camera device open success!!!, [HRESULT : 0x%x]\n", hr);
}

// SVCAM_CMD_CLOSE
void svcam_device_close(SVCamState* state)
{
    HRESULT hr;
    SVCamParam *param = state->thread->param;
    param->top = 0;
    hr = SVCamCtrl(HWC_CLOSE, 0, NULL);
    if (FAILED(hr)) {
        param->errCode = EINVAL;
        ERR("camera device close failed!!!, [HRESULT : 0x%x]\n", hr);
    }
    FreeLibrary(g_hInst);
    TRACE("camera device close success!!!, [HRESULT : 0x%x]\n", hr);
}

// SVCAM_CMD_START_PREVIEW
void svcam_device_start_preview(SVCamState* state)
{
    HRESULT hr;
    uint32_t width, height;
    SVCamParam *param = state->thread->param;
    TRACE("svcam_device_start_preview\n");
    param->top = 0;
    hr = SVCamCtrl(HWC_START, 0, NULL);
    if (FAILED(hr)) {
        param->errCode = EINVAL;
        ERR("start preview failed!!!, [HRESULT : 0x%x]\n", hr);
        return;
    }
    pthread_mutex_lock(&state->thread->mutex_lock);
    state->streamon = 1;
    pthread_mutex_unlock(&state->thread->mutex_lock);

    width = supported_dst_frames[cur_frame_idx].width;
    height = supported_dst_frames[cur_frame_idx].height;
    state->buf_size = height * ((width * supported_dst_pixfmts[cur_fmt_idx].bpp) >> 3);
}

// SVCAM_CMD_STOP_PREVIEW
void svcam_device_stop_preview(SVCamState* state)
{
    HRESULT hr;
    SVCamParam *param = state->thread->param;
    TRACE("svcam_device_stop_preview\n");
    param->top = 0;
    hr = SVCamCtrl(HWC_STOP, 0, NULL);
    if (FAILED(hr)) {
        param->errCode = EINVAL;
        ERR("stop preview failed!!!, [HRESULT : 0x%x]\n", hr);
    }
    pthread_mutex_lock(&state->thread->mutex_lock);
    state->streamon = 0;
    pthread_mutex_unlock(&state->thread->mutex_lock);
    state->buf_size = 0;
}

// SVCAM_CMD_S_PARAM
void svcam_device_s_param(SVCamState* state)
{
    SVCamParam *param = state->thread->param;

    param->top = 0;
    TRACE("setting fps : %d/%d\n", param->stack[0], param->stack[1]);
}

// SVCAM_CMD_G_PARAM
void svcam_device_g_param(SVCamState* state)
{
    SVCamParam *param = state->thread->param;

    param->top = 0;
    TRACE("getting fps : 30/1\n");

    param->stack[0] = 0x1000; // V4L2_CAP_TIMEPERFRAME
    param->stack[1] = 1; // numerator;
    param->stack[2] = 30; // denominator;
}

// SVCAM_CMD_S_FMT
void svcam_device_s_fmt(SVCamState* state)
{
    uint32_t width, height, pixfmt, pidx, fidx;
    SVCamParam *param = state->thread->param;

    param->top = 0;
    width = param->stack[0];        // width
    height = param->stack[1];       // height
    pixfmt = param->stack[2];       // pixelformat

    for (fidx = 0; fidx < ARRAY_SIZE(supported_dst_frames); fidx++) {
        if ((supported_dst_frames[fidx].width == width) &&
                (supported_dst_frames[fidx].height == height)) {
            break;
        }
    }
    if (fidx == ARRAY_SIZE(supported_dst_frames)) {
        param->errCode = EINVAL;
        return;
    }
    for (pidx = 0; pidx < ARRAY_SIZE(supported_dst_pixfmts); pidx++) {
        if (supported_dst_pixfmts[pidx].fmt == pixfmt) {
            break;
        }
    }
    if (pidx == ARRAY_SIZE(supported_dst_pixfmts)) {
        param->errCode = EINVAL;
        return;
    }

    if ((supported_dst_frames[cur_frame_idx].width != width) &&
            (supported_dst_frames[cur_frame_idx].height != height)) {
        HWCParam inParam = {0,};
        inParam.val1 = width;
        inParam.val2 = height;
        HRESULT hr = SVCamCtrl(HWC_S_FMT, sizeof(HWCParam), &inParam);
        if (FAILED(hr)) {
            param->errCode = EINVAL;
            return;
        }
    }

    param->stack[0] = width;
    param->stack[1] = height;
    param->stack[2] = 1; // V4L2_FIELD_NONE
    param->stack[3] = pixfmt;
    // bytes per line = (width * bpp) / 8
    param->stack[4] = (width * supported_dst_pixfmts[pidx].bpp) >> 3;
    param->stack[5] = param->stack[4] * height; // height * bytesperline
    param->stack[6] = 0;
    param->stack[7] = 0;

    cur_frame_idx = fidx;
    cur_fmt_idx = pidx;
}

// SVCAM_CMD_G_FMT
void svcam_device_g_fmt(SVCamState* state)
{
    SVCamParam *param = state->thread->param;

    param->top = 0;

    param->stack[0] = supported_dst_frames[cur_frame_idx].width;    // width
    param->stack[1] = supported_dst_frames[cur_frame_idx].height;   // height
    param->stack[2] = 1; // V4L2_FIELD_NONE
    param->stack[3] = supported_dst_pixfmts[cur_fmt_idx].fmt;   // pixelformat
    // bytes per line = (width * bpp) / 8
    param->stack[4] = (param->stack[0] * supported_dst_pixfmts[cur_fmt_idx].bpp) >> 3;
    param->stack[5] = param->stack[1] * param->stack[4];    // height * bytesperline
    param->stack[6] = 0;
    param->stack[7] = 0;
}

void svcam_device_try_fmt(SVCamState* state)
{
    uint32_t width, height, pixfmt, i;
    SVCamParam *param = state->thread->param;

    param->top = 0;
    width = param->stack[0];        // width
    height = param->stack[1];       // height
    pixfmt = param->stack[2];       // pixelformat

    for (i = 0; i < ARRAY_SIZE(supported_dst_frames); i++) {
        if ((supported_dst_frames[i].width == width) &&
                (supported_dst_frames[i].height == height)) {
            break;
        }
    }
    if (i == ARRAY_SIZE(supported_dst_frames)) {
        param->errCode = EINVAL;
        return;
    }
    for (i = 0; i < ARRAY_SIZE(supported_dst_pixfmts); i++) {
        if (supported_dst_pixfmts[i].fmt == pixfmt) {
            break;
        }
    }
    if (i == ARRAY_SIZE(supported_dst_pixfmts)) {
        param->errCode = EINVAL;
        return;
    }

    param->stack[0] = width;
    param->stack[1] = height;
    param->stack[2] = 1; // V4L2_FIELD_NONE
    param->stack[3] = pixfmt;
    // bytes per line = (width * bpp) / 8
    param->stack[4] = (width * supported_dst_pixfmts[i].bpp) >> 3;
    param->stack[5] = param->stack[4] * height; // height * bytesperline
    param->stack[6] = 0;
    param->stack[7] = 0;
}

void svcam_device_enum_fmt(SVCamState* state)
{
    uint32_t index;
    SVCamParam *param = state->thread->param;

    param->top = 0;
    index = param->stack[0];

    if (index >= ARRAY_SIZE(supported_dst_pixfmts)) {
        param->errCode = EINVAL;
        return;
    }
    param->stack[1] = 0;                            // flags = NONE;
    param->stack[2] = supported_dst_pixfmts[index].fmt; // pixelformat;
    /* set description */
    switch (supported_dst_pixfmts[index].fmt) {
    case V4L2_PIX_FMT_YUYV:
        memcpy(&param->stack[3], "YUY2", 32);
        break;
    case V4L2_PIX_FMT_YUV420:
        memcpy(&param->stack[3], "YU12", 32);
        break;
    case V4L2_PIX_FMT_YVU420:
        memcpy(&param->stack[3], "YV12", 32);
        break;
    }
}

void svcam_device_qctrl(SVCamState* state)
{
    HRESULT hr;
    uint32_t id, i;
    HWCParam inParam = {0,};
    char name[32] = {0,};
    SVCamParam *param = state->thread->param;

    param->top = 0;
    id = param->stack[0];

    switch (id) {
    case V4L2_CID_BRIGHTNESS:
        TRACE("V4L2_CID_BRIGHTNESS\n");
        inParam.val1 = VideoProcAmp_Brightness;
        memcpy((void*)name, (void*)"brightness", 32);
        i = 0;
        break;
    case V4L2_CID_CONTRAST:
        TRACE("V4L2_CID_CONTRAST\n");
        inParam.val1 = VideoProcAmp_Contrast;
        memcpy((void*)name, (void*)"contrast", 32);
        i = 1;
        break;
    case V4L2_CID_SATURATION:
        TRACE("V4L2_CID_SATURATION\n");
        inParam.val1 = VideoProcAmp_Saturation;
        memcpy((void*)name, (void*)"saturation", 32);
        i = 2;
        break;
    case V4L2_CID_SHARPNESS:
        TRACE("V4L2_CID_SHARPNESS\n");
        inParam.val1 = VideoProcAmp_Sharpness;
        memcpy((void*)name, (void*)"sharpness", 32);
        i = 3;
        break;
    default:
        param->errCode = EINVAL;
        return;
    }
    hr = SVCamCtrl(HWC_QCTRL, sizeof(inParam), &inParam);
    if (FAILED(hr)) {
        param->errCode = EINVAL;
        ERR("failed to query video controls [HRESULT : 0x%x]\n", hr);
        return;
    } else {
        qctrl_tbl[i].hit = 1;
        qctrl_tbl[i].min = inParam.val2;
        qctrl_tbl[i].max = inParam.val3;
        qctrl_tbl[i].step = inParam.val4;
        qctrl_tbl[i].init_val = inParam.val5;

        if ((qctrl_tbl[i].min + qctrl_tbl[i].max) == 0) {
            inParam.val2 = 0;
        } else {
            inParam.val2 = (qctrl_tbl[i].min + qctrl_tbl[i].max) / 2;
        }
        hr = SVCamCtrl(HWC_S_CTRL, sizeof(inParam), &inParam);
        if (FAILED(hr)) {
            param->errCode = EINVAL;
            ERR("failed to set video control value, [HRESULT : 0x%x]\n", hr);
            return;
        }
    }

    param->stack[0] = id;
    param->stack[1] = SVCAM_CTRL_VALUE_MIN; // minimum
    param->stack[2] = SVCAM_CTRL_VALUE_MAX; // maximum
    param->stack[3] = SVCAM_CTRL_VALUE_STEP;// step
    param->stack[4] = SVCAM_CTRL_VALUE_MID; // default_value
    param->stack[5] = V4L2_CTRL_FLAG_SLIDER;
    /* name field setting */
    memcpy(&param->stack[6], (void*)name, sizeof(name)/sizeof(name[0]));
}

void svcam_device_s_ctrl(SVCamState* state)
{
    HRESULT hr;
    uint32_t i;
    HWCParam inParam = {0,};
    SVCamParam *param = state->thread->param;

    param->top = 0;

    switch (param->stack[0]) {
    case V4L2_CID_BRIGHTNESS:
        i = 0;
        inParam.val1 = VideoProcAmp_Brightness;
        break;
    case V4L2_CID_CONTRAST:
        i = 1;
        inParam.val1 = VideoProcAmp_Contrast;
        break;
    case V4L2_CID_SATURATION:
        i = 2;
        inParam.val1 = VideoProcAmp_Saturation;
        break;
    case V4L2_CID_SHARPNESS:
        i = 3;
        inParam.val1 = VideoProcAmp_Sharpness;
        break;
    default:
        param->errCode = EINVAL;
        return;
    }
    inParam.val2 = value_convert_from_guest(qctrl_tbl[i].min,
            qctrl_tbl[i].max, (long)param->stack[1]);
    hr = SVCamCtrl(HWC_S_CTRL, sizeof(inParam), &inParam);
    if (FAILED(hr)) {
        param->errCode = EINVAL;
        ERR("failed to set video control value, [HRESULT : 0x%x]\n", hr);
        return;
    }
}

void svcam_device_g_ctrl(SVCamState* state)
{
    HRESULT hr;
    uint32_t i;
    HWCParam inParam = {0,};
    SVCamParam *param = state->thread->param;

    param->top = 0;
    switch (param->stack[0]) {
    case V4L2_CID_BRIGHTNESS:
        i = 0;
        inParam.val1 = VideoProcAmp_Brightness;
        break;
    case V4L2_CID_CONTRAST:
        i = 1;
        inParam.val1 = VideoProcAmp_Contrast;
        break;
    case V4L2_CID_SATURATION:
        i = 2;
        inParam.val1 = VideoProcAmp_Saturation;
        break;
    case V4L2_CID_SHARPNESS:
        i = 3;
        inParam.val1 = VideoProcAmp_Sharpness;
        break;
    default:
        param->errCode = EINVAL;
        return;
    }

    hr = SVCamCtrl(HWC_G_CTRL, sizeof(inParam), &inParam);
    if (FAILED(hr)) {
        param->errCode = EINVAL;
        ERR("failed to get video control value!!!, [HRESULT : 0x%x]\n", hr);
        return;
    }
    param->stack[0] = (uint32_t)value_convert_to_guest(qctrl_tbl[i].min,
                qctrl_tbl[i].max, inParam.val2);
}

void svcam_device_enum_fsizes(SVCamState* state)
{
    uint32_t index, pixfmt, i;
    SVCamParam *param = state->thread->param;

    param->top = 0;
    index = param->stack[0];
    pixfmt = param->stack[1];

    if (index >= ARRAY_SIZE(supported_dst_frames)) {
        param->errCode = EINVAL;
        return;
    }
    for (i = 0; i < ARRAY_SIZE(supported_dst_pixfmts); i++) {
        if (supported_dst_pixfmts[i].fmt == pixfmt)
            break;
    }

    if (i == ARRAY_SIZE(supported_dst_pixfmts)) {
        param->errCode = EINVAL;
        return;
    }

    param->stack[0] = supported_dst_frames[index].width;
    param->stack[1] = supported_dst_frames[index].height;
}

void svcam_device_enum_fintv(SVCamState* state)
{
    SVCamParam *param = state->thread->param;

    param->top = 0;

    // switch by index(param->stack[0])
    switch (param->stack[0]) {
    case 0:
        param->stack[1] = 30;   // denominator
        break;
    default:
        param->errCode = EINVAL;
        return;
    }
    param->stack[0] = 1;    // numerator
}

void v4lconvert_yuyv_to_yuv420(const unsigned char *src, unsigned char *dest,
        uint32_t width, uint32_t height, uint32_t yvu)
{
    uint32_t i, j;
    const unsigned char *src1;
    unsigned char *udest, *vdest;

    /* copy the Y values */
    src1 = src;
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j += 2) {
            *dest++ = src1[0];
            *dest++ = src1[2];
            src1 += 4;
        }
    }

    /* copy the U and V values */
    src++;              /* point to V */
    src1 = src + width * 2;     /* next line */
    if (yvu) {
        vdest = dest;
        udest = dest + width * height / 4;
    } else {
        udest = dest;
        vdest = dest + width * height / 4;
    }
    for (i = 0; i < height; i += 2) {
        for (j = 0; j < width; j += 2) {
            *udest++ = ((int) src[0] + src1[0]) / 2;    /* U */
            *vdest++ = ((int) src[2] + src1[2]) / 2;    /* V */
            src += 4;
            src1 += 4;
        }
        src = src1;
        src1 += width * 2;
    }
}
