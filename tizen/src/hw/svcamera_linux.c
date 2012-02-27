/*
 * Virtual Camera device(PCI) for Linux host.
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
#include "qemu-common.h"
#include "svcamera.h"
#include "pci.h"
#include "kvm.h"
#include "tizen/src/debug_ch.h"

#include <linux/videodev2.h>

#include <libv4l2.h>
#include <libv4lconvert.h>

MULTI_DEBUG_CHANNEL(tizen, camera_linux);

static int v4l2_fd;
static int convert_trial;

static struct v4l2_format dst_fmt;

static int xioctl(int fd, int req, void *arg)
{
    int r;

    do {
        r = v4l2_ioctl(fd, req, arg);
    } while ( r < 0 && errno == EINTR);

    return r;
}

#define SVCAM_CTRL_VALUE_MAX        20
#define SVCAM_CTRL_VALUE_MIN        1
#define SVCAM_CTRL_VALUE_MID        10
#define SVCAM_CTRL_VALUE_STEP       1

struct svcam_qctrl {
    uint32_t id;
    uint32_t hit;
    int32_t min;
    int32_t max;
    int32_t step;
    int32_t init_val;
};

static struct svcam_qctrl qctrl_tbl[] = {
    { V4L2_CID_BRIGHTNESS, 0, },
    { V4L2_CID_CONTRAST, 0, },
    { V4L2_CID_SATURATION,0, },
    { V4L2_CID_SHARPNESS, 0, },
};

static void svcam_reset_controls(void)
{
    uint32_t i;
    for (i = 0; i < ARRAY_SIZE(qctrl_tbl); i++) {
        if (qctrl_tbl[i].hit) {
            struct v4l2_control ctrl = {0,};
            ctrl.id = qctrl_tbl[i].id;
            ctrl.value = qctrl_tbl[i].init_val;
            if (xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
                ERR("failed to set video control value while reset values : %s\n", strerror(errno));
            }
        }
    }
}

static int32_t value_convert_from_guest(int32_t min, int32_t max, int32_t value)
{
    double rate = 0.0;
    int32_t dist = 0, ret = 0;

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

static int32_t value_convert_to_guest(int32_t min, int32_t max, int32_t value)
{
    double rate  = 0.0;
    int32_t dist = 0, ret = 0;

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

static int __v4l2_grab(SVCamState *state)
{
    fd_set fds;
    static uint32_t index = 0;
    struct timeval tv;
    void *buf;
    int ret;
    
    FD_ZERO(&fds);
    FD_SET(v4l2_fd, &fds);

    tv.tv_sec = 2;
    tv.tv_usec = 0;

    ret = select(v4l2_fd + 1, &fds, NULL, NULL, &tv);
    if ( ret < 0) {
        if (errno == EINTR)
            return 0;
        ERR("select : %s\n", strerror(errno));
        return -1;
    }
    if (!ret) {
        WARN("Timed out\n");
        return 0;
    }

    if (!v4l2_fd) {
        WARN("file descriptor is closed or not opened \n");
        return -1;
    }

    buf = state->vaddr + (state->buf_size * index);
    ret = v4l2_read(v4l2_fd, buf, state->buf_size);
    if ( ret < 0) {
        switch (errno) {
        case EINVAL:
        case ENOMEM:
            ERR("v4l2_read failed : %s\n", strerror(errno));
            return -1;
        case EAGAIN:
        case EIO:
        case EINTR:
        default:
            if (convert_trial-- == -1) {
                ERR("Try count for v4l2_read is exceeded\n");
                return -1;
            }
            return 0;
        }
    }

    index = !index;

    pthread_mutex_lock(&state->thread->mutex_lock);
    if (state->streamon) {
        if (state->req_frame) {
            qemu_irq_raise(state->dev.irq[2]);
            state->req_frame = 0;
        }
        ret = 1;
    } else {
        ret = -1;
    }
    pthread_mutex_unlock(&state->thread->mutex_lock);

    return ret;
}

// Worker thread
static void *svcam_worker_thread(void *thread_param)
{
    SVCamThreadInfo* thread = (SVCamThreadInfo*)thread_param;

wait_worker_thread:
    pthread_mutex_lock(&thread->mutex_lock);
    thread->state->streamon = 0;
    convert_trial = 10;
    pthread_cond_wait(&thread->thread_cond, &thread->mutex_lock);
    pthread_mutex_unlock(&thread->mutex_lock);
    INFO("Streaming on ......\n");

    while (1)
    {
        pthread_mutex_lock(&thread->mutex_lock);
        if (thread->state->streamon) {
            pthread_mutex_unlock(&thread->mutex_lock);
            if (__v4l2_grab(thread->state) < 0) {
                INFO("...... Streaming off\n");
                goto wait_worker_thread;
            }
        } else {
            pthread_mutex_unlock(&thread->mutex_lock);
            goto wait_worker_thread;
        }
    }
    pthread_exit(0);
}

void svcam_device_init(SVCamState* state)
{
    SVCamThreadInfo *thread = state->thread;

    if (pthread_cond_init(&thread->thread_cond, NULL)) {
        ERR("failed to initialize thread condition\n");
        exit(61);
    }
    if (pthread_mutex_init(&thread->mutex_lock, NULL)) {
        ERR("failed to initialize mutex\n");
        exit(62);
    }

    if (pthread_create(&thread->thread_id, NULL, svcam_worker_thread, (void*)thread) != 0) {
        perror("failed to create a worker thread for webcam connection\n");
        exit(63);
    }
}

// SVCAM_CMD_OPEN
void svcam_device_open(SVCamState* state)
{
    struct v4l2_capability cap;
    SVCamParam *param = state->thread->param;

    param->top = 0;
    v4l2_fd = v4l2_open("/dev/video0", O_RDWR | O_NONBLOCK);
    if (v4l2_fd < 0) {
        ERR("v4l2 device open failed.(/dev/video0)\n");
        param->errCode = EINVAL;
        return;
    }
    if (xioctl(v4l2_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        ERR("Could not qeury video capabilities\n");
        v4l2_close(v4l2_fd);
        param->errCode = EINVAL;
        return;
    }
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
            !(cap.capabilities & V4L2_CAP_STREAMING)) {
        ERR("Not supported video driver.\n");
        v4l2_close(v4l2_fd);
        param->errCode = EINVAL;
        return;
    }

    memset(&dst_fmt, 0, sizeof(dst_fmt));
    INFO("Opened\n");
}

// SVCAM_CMD_START_PREVIEW
void svcam_device_start_preview(SVCamState* state)
{
    pthread_mutex_lock(&state->thread->mutex_lock);
    state->streamon = 1;
    state->buf_size = dst_fmt.fmt.pix.sizeimage;
    if (pthread_cond_signal(&state->thread->thread_cond))
        ERR("failed to send a signal to the worker thread\n");
    pthread_mutex_unlock(&state->thread->mutex_lock);
}

// SVCAM_CMD_STOP_PREVIEW
void svcam_device_stop_preview(SVCamState* state)
{
    pthread_mutex_lock(&state->thread->mutex_lock);
    state->streamon = 0;
    state->buf_size = 0;
    pthread_mutex_unlock(&state->thread->mutex_lock);
    sleep(0);
}

void svcam_device_s_param(SVCamState* state)
{
    struct v4l2_streamparm sp;
    SVCamParam *param = state->thread->param;

    param->top = 0;
    memset(&sp, 0, sizeof(struct v4l2_streamparm));
    sp.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    sp.parm.capture.timeperframe.numerator = param->stack[0];
    sp.parm.capture.timeperframe.denominator = param->stack[1];

    if (xioctl(v4l2_fd, VIDIOC_S_PARM, &sp) < 0) {
        ERR("failed to set FPS: %s\n", strerror(errno));
        param->errCode = errno;
    }
}

void svcam_device_g_param(SVCamState* state)
{
    struct v4l2_streamparm sp;
    SVCamParam *param = state->thread->param;
    
    param->top = 0;
    memset(&sp, 0, sizeof(struct v4l2_streamparm));
    sp.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(v4l2_fd, VIDIOC_G_PARM, &sp) < 0) {
        ERR("failed to get FPS: %s\n", strerror(errno));
        param->errCode = errno;
        return;
    }
    param->stack[0] = sp.parm.capture.capability;
    param->stack[1] = sp.parm.capture.timeperframe.numerator;
    param->stack[2] = sp.parm.capture.timeperframe.denominator;
}

void svcam_device_s_fmt(SVCamState* state)
{
    SVCamParam *param = state->thread->param;

    param->top = 0;
    memset(&dst_fmt, 0, sizeof(struct v4l2_format));
    dst_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    dst_fmt.fmt.pix.width = param->stack[0];
    dst_fmt.fmt.pix.height = param->stack[1];
    dst_fmt.fmt.pix.pixelformat = param->stack[2];
    dst_fmt.fmt.pix.field = param->stack[3];

    if (xioctl(v4l2_fd, VIDIOC_S_FMT, &dst_fmt) < 0) {
        ERR("failed to set video format: %s\n", strerror(errno));
        param->errCode = errno;
        return;
    }

    param->stack[0] = dst_fmt.fmt.pix.width;
    param->stack[1] = dst_fmt.fmt.pix.height;
    param->stack[2] = dst_fmt.fmt.pix.field;
    param->stack[3] = dst_fmt.fmt.pix.pixelformat;
    param->stack[4] = dst_fmt.fmt.pix.bytesperline;
    param->stack[5] = dst_fmt.fmt.pix.sizeimage;
    param->stack[6] = dst_fmt.fmt.pix.colorspace;
    param->stack[7] = dst_fmt.fmt.pix.priv;
}

void svcam_device_g_fmt(SVCamState* state)
{
    struct v4l2_format format;
    SVCamParam *param = state->thread->param;

    param->top = 0;
    memset(&format, 0, sizeof(struct v4l2_format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(v4l2_fd, VIDIOC_G_FMT, &format) < 0) {
        ERR("failed to get video format: %s\n", strerror(errno));
        param->errCode = errno;     
    } else {
        param->stack[0] = format.fmt.pix.width;
        param->stack[1] = format.fmt.pix.height;
        param->stack[2] = format.fmt.pix.field;
        param->stack[3] = format.fmt.pix.pixelformat;
        param->stack[4] = format.fmt.pix.bytesperline;
        param->stack[5] = format.fmt.pix.sizeimage;
        param->stack[6] = format.fmt.pix.colorspace;
        param->stack[7] = format.fmt.pix.priv;
        memcpy(&dst_fmt, &format, sizeof(format));
    }
}

void svcam_device_try_fmt(SVCamState* state)
{
    struct v4l2_format format;
    SVCamParam *param = state->thread->param;

    param->top = 0;
    memset(&format, 0, sizeof(struct v4l2_format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = param->stack[0];
    format.fmt.pix.height = param->stack[1];
    format.fmt.pix.pixelformat = param->stack[2];
    format.fmt.pix.field = param->stack[3];

    if (xioctl(v4l2_fd, VIDIOC_TRY_FMT, &format) < 0) {
        ERR("failed to check video format: %s\n", strerror(errno));
        param->errCode = errno;
        return;
    }
    param->stack[0] = format.fmt.pix.width;
    param->stack[1] = format.fmt.pix.height;
    param->stack[2] = format.fmt.pix.field;
    param->stack[3] = format.fmt.pix.pixelformat;
    param->stack[4] = format.fmt.pix.bytesperline;
    param->stack[5] = format.fmt.pix.sizeimage;
    param->stack[6] = format.fmt.pix.colorspace;
    param->stack[7] = format.fmt.pix.priv;
}

void svcam_device_enum_fmt(SVCamState* state)
{
    struct v4l2_fmtdesc format;
    SVCamParam *param = state->thread->param;

    param->top = 0;
    memset(&format, 0, sizeof(struct v4l2_fmtdesc));
    format.index = param->stack[0];
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(v4l2_fd, VIDIOC_ENUM_FMT, &format) < 0) {
        if (errno != EINVAL)
            ERR("failed to enumerate video formats: %s\n", strerror(errno));
        param->errCode = errno;
        return;
    }
    param->stack[0] = format.index;
    param->stack[1] = format.flags;
    param->stack[2] = format.pixelformat;
    /* set description */
    memcpy(&param->stack[3], format.description, sizeof(format.description));
}

void svcam_device_qctrl(SVCamState* state)
{
    uint32_t i;
    char name[32] = {0,};
    struct v4l2_queryctrl ctrl;
    SVCamParam *param = state->thread->param;

    param->top = 0;
    memset(&ctrl, 0, sizeof(struct v4l2_queryctrl));
    ctrl.id = param->stack[0];

    switch (ctrl.id) {
    case V4L2_CID_BRIGHTNESS:
        TRACE("Query : BRIGHTNESS\n");
        memcpy((void*)name, (void*)"brightness", 32);
        i = 0;
        break;
    case V4L2_CID_CONTRAST:
        TRACE("Query : CONTRAST\n");
        memcpy((void*)name, (void*)"contrast", 32);
        i = 1;
        break;
    case V4L2_CID_SATURATION:
        TRACE("Query : SATURATION\n");
        memcpy((void*)name, (void*)"saturation", 32);
        i = 2;
        break;
    case V4L2_CID_SHARPNESS:
        TRACE("Query : SHARPNESS\n");
        memcpy((void*)name, (void*)"sharpness", 32);
        i = 3;
        break;
    default:
        param->errCode = EINVAL;
        return;
    }

    if (xioctl(v4l2_fd, VIDIOC_QUERYCTRL, &ctrl) < 0) {
        if (errno != EINVAL)
            ERR("failed to query video controls : %s\n", strerror(errno));
        param->errCode = errno;
        return;
    } else {
        struct v4l2_control sctrl;
        memset(&sctrl, 0, sizeof(struct v4l2_control));
        sctrl.id = ctrl.id;
        if ((ctrl.maximum + ctrl.minimum) == 0) {
            sctrl.value = 0;
        } else {
            sctrl.value = (ctrl.maximum + ctrl.minimum) / 2;
        }
        if (xioctl(v4l2_fd, VIDIOC_S_CTRL, &sctrl) < 0) {
            ERR("failed to set video control value : %s\n", strerror(errno));
            param->errCode = errno;
            return;
        }
        qctrl_tbl[i].hit = 1;
        qctrl_tbl[i].min = ctrl.minimum;
        qctrl_tbl[i].max = ctrl.maximum;
        qctrl_tbl[i].step = ctrl.step;
        qctrl_tbl[i].init_val = ctrl.default_value;
    }

    // set fixed values by FW configuration file
    param->stack[0] = ctrl.id;
    param->stack[1] = SVCAM_CTRL_VALUE_MIN; // minimum
    param->stack[2] = SVCAM_CTRL_VALUE_MAX; // maximum
    param->stack[3] = SVCAM_CTRL_VALUE_STEP;// step
    param->stack[4] = SVCAM_CTRL_VALUE_MID; // default_value
    param->stack[5] = ctrl.flags;
    /* name field setting */
    memcpy(&param->stack[6], (void*)name, sizeof(ctrl.name));
}

void svcam_device_s_ctrl(SVCamState* state)
{
    uint32_t i;
    struct v4l2_control ctrl;
    SVCamParam *param = state->thread->param;

    param->top = 0;
    memset(&ctrl, 0, sizeof(struct v4l2_control));
    ctrl.id = param->stack[0];

    switch (ctrl.id) {
    case V4L2_CID_BRIGHTNESS:
        i = 0;
        TRACE("%d is set to the value of the BRIGHTNESS\n", param->stack[1]);
        break;
    case V4L2_CID_CONTRAST:
        i = 1;
        TRACE("%d is set to the value of the CONTRAST\n", param->stack[1]);
        break;
    case V4L2_CID_SATURATION:
        i = 2;
        TRACE("%d is set to the value of the SATURATION\n", param->stack[1]);
        break;
    case V4L2_CID_SHARPNESS:
        i = 3;
        TRACE("%d is set to the value of the SHARPNESS\n", param->stack[1]);
        break;
    default:
        ERR("our emulator does not support this control : 0x%x\n", ctrl.id);
        param->errCode = EINVAL;
        return;
    }

    ctrl.value = value_convert_from_guest(qctrl_tbl[i].min,
            qctrl_tbl[i].max, param->stack[1]);
    if (xioctl(v4l2_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        ERR("failed to set video control value : value(%d), %s\n", param->stack[1], strerror(errno));
        param->errCode = errno;
        return;
    }
}

void svcam_device_g_ctrl(SVCamState* state)
{
    uint32_t i;
    struct v4l2_control ctrl;
    SVCamParam *param = state->thread->param;

    param->top = 0;
    memset(&ctrl, 0, sizeof(struct v4l2_control));
    ctrl.id = param->stack[0];

    switch (ctrl.id) {
    case V4L2_CID_BRIGHTNESS:
        TRACE("Gets the value of the BRIGHTNESS\n");
        i = 0;
        break;
    case V4L2_CID_CONTRAST:
        TRACE("Gets the value of the CONTRAST\n");
        i = 1;
        break;
    case V4L2_CID_SATURATION:
        TRACE("Gets the value of the SATURATION\n");
        i = 2;
        break;
    case V4L2_CID_SHARPNESS:
        TRACE("Gets the value of the SHARPNESS\n");
        i = 3;
        break;
    default:
        ERR("our emulator does not support this control : 0x%x\n", ctrl.id);
        param->errCode = EINVAL;
        return;
    }

    if (xioctl(v4l2_fd, VIDIOC_G_CTRL, &ctrl) < 0) {
        ERR("failed to get video control value : %s\n", strerror(errno));
        param->errCode = errno;
        return;
    }
    param->stack[0] = value_convert_to_guest(qctrl_tbl[i].min,
            qctrl_tbl[i].max, ctrl.value);
    TRACE("Value : %d\n", param->stack[0]);
}

void svcam_device_enum_fsizes(SVCamState* state)
{
    struct v4l2_frmsizeenum fsize;
    SVCamParam *param = state->thread->param;

    param->top = 0;
    memset(&fsize, 0, sizeof(struct v4l2_frmsizeenum));
    fsize.index = param->stack[0];
    fsize.pixel_format = param->stack[1];

    if (xioctl(v4l2_fd, VIDIOC_ENUM_FRAMESIZES, &fsize) < 0) {
        if (errno != EINVAL)
            ERR("failed to get frame sizes : %s\n", strerror(errno));
        param->errCode = errno;
        return;
    }

    if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
        param->stack[0] = fsize.discrete.width;
        param->stack[1] = fsize.discrete.height;
    } else {
        param->errCode = EINVAL;
        ERR("Not Supported mode, we only support DISCRETE\n");
    }
}

void svcam_device_enum_fintv(SVCamState* state)
{
    struct v4l2_frmivalenum ival;
    SVCamParam *param = state->thread->param;

    param->top = 0;
    memset(&ival, 0, sizeof(struct v4l2_frmivalenum));
    ival.index = param->stack[0];
    ival.pixel_format = param->stack[1];
    ival.width = param->stack[2];
    ival.height = param->stack[3];

    if (xioctl(v4l2_fd, VIDIOC_ENUM_FRAMEINTERVALS, &ival) < 0) {
        if (errno != EINVAL)
            ERR("failed to get frame intervals : %s\n", strerror(errno));
        param->errCode = errno;
        return;
    }

    if (ival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
        param->stack[0] = ival.discrete.numerator;
        param->stack[1] = ival.discrete.denominator;
    } else {
        param->errCode = EINVAL;
        ERR("Not Supported mode, we only support DISCRETE\n");
    }
}

// SVCAM_CMD_CLOSE
void svcam_device_close(SVCamState* state)
{
    pthread_mutex_lock(&state->thread->mutex_lock);
    state->streamon = 0;
    pthread_mutex_unlock(&state->thread->mutex_lock);

    svcam_reset_controls();

    v4l2_close(v4l2_fd);
    v4l2_fd = 0;
    INFO("Closed\n");
}
