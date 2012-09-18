/*
 * Virtual Codec device
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Contact:
 *  Kitae Kim <kt920.kim@samsung.com>
 *  SeokYeon Hwang <syeon.hwang@samsung.com>
 *  DongKyun Yun <dk77.yun@samsung.com>
 *  YeongKyoon Lee <yeongkyoon.lee@samsung.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Contributors:
 * - S-Core Co., Ltd
 *
 */

#include "maru_codec.h"
#include "qemu-common.h"

#define MARU_CODEC_DEV_NAME     "codec"
#define MARU_CODEC_VERSION      "1.3"

/*  Needs 16M to support 1920x1080 video resolution.
 *  Output size for encoding has to be greater than (width * height * 6)
 */
#define MARU_CODEC_MEM_SIZE     (2 * 16 * 1024 * 1024)
#define MARU_CODEC_REG_SIZE     (256)

#define MARU_ROUND_UP_16(num)   (((num) + 15) & ~15)
#define CODEC_IRQ 0x7f

/* define debug channel */
MULTI_DEBUG_CHANNEL(qemu, marucodec);

void codec_thread_init(SVCodecState *s)
{
    TRACE("Enter, %s\n", __func__);

    qemu_thread_create(&s->thread_id, codec_worker_thread, (void *)s);

    TRACE("Leave, %s\n", __func__);
}

void codec_thread_exit(SVCodecState *s)
{
    TRACE("Enter, %s\n", __func__);

    TRACE("destroy mutex and conditional.\n");
    qemu_mutex_destroy(&s->thread_mutex);
    qemu_cond_destroy(&s->thread_cond);

/*  qemu_thread_exit((void*)0); */
    TRACE("Leave, %s\n", __func__);
}

void wake_codec_worker_thread(SVCodecState *s)
{
    TRACE("Enter, %s\n", __func__);

    qemu_cond_signal(&s->thread_cond);
    TRACE("sent a conditional signal to a worker thread.\n");

    TRACE("Leave, %s\n", __func__);
}

void *codec_worker_thread(void *opaque)
{
    SVCodecState *s = (SVCodecState *)opaque;
    AVCodecContext *avctx;

    TRACE("Enter, %s\n", __func__);
    qemu_mutex_lock(&s->thread_mutex);

    while (1) {
        TRACE("wait conditional signal.\n");
        qemu_cond_wait(&s->thread_cond, &s->thread_mutex);

        TRACE("wake up a worker thread.\n");
        avctx = s->ctx_arr[s->codec_param.ctx_index].avctx;
        /* decode and encode */
        if (avctx) {
            if (avctx->codec->decode) {
                decode_codec(s);
            } else {
                encode_codec(s);
            }
        } else {
            ERR("there is a synchrous problem "
                "between each context.\n");
            continue;
        }

        s->thread_state = CODEC_IRQ;
        qemu_bh_schedule(s->tx_bh);
    }
    qemu_mutex_unlock(&s->thread_mutex);
    TRACE("Leave, %s\n", __func__);
}

int decode_codec(SVCodecState *s)
{
    AVCodecContext *avctx;
    uint32_t len = 0, ctx_index;

    TRACE("Enter, %s\n", __func__);
    qemu_mutex_unlock(&s->thread_mutex);
/*  qemu_mutex_lock(&s->thread_mutex); */

    ctx_index = s->codec_param.ctx_index;
    avctx = s->ctx_arr[ctx_index].avctx;
    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        len = qemu_avcodec_decode_video(s, ctx_index);
    } else {
        len = qemu_avcodec_decode_audio(s, ctx_index);
    }

/*  qemu_mutex_unlock(&s->thread_mutex); */
    qemu_mutex_lock(&s->thread_mutex);

    TRACE("Leave, %s\n", __func__);
    return len;
}

int encode_codec(SVCodecState *s)
{
    AVCodecContext *avctx;
    uint32_t len = 0, ctx_index;

    TRACE("Enter, %s\n", __func__);

    qemu_mutex_unlock(&s->thread_mutex);
/*  qemu_mutex_lock(&s->thread_mutex); */

    ctx_index = s->codec_param.ctx_index;
    avctx = s->ctx_arr[ctx_index].avctx;
    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        len = qemu_avcodec_encode_video(s, ctx_index);
    } else {
        len = qemu_avcodec_encode_audio(s, ctx_index);
    }

/*  qemu_mutex_unlock(&s->thread_mutex); */
    qemu_mutex_lock(&s->thread_mutex);

    TRACE("Leave, %s\n", __func__);
    return len;
}

static int qemu_serialize_rational(const AVRational *elem, uint8_t *buff)
{
    int size = 0;

    memcpy(buff + size, &elem->num, sizeof(elem->num));
    size += sizeof(elem->num);
    memcpy(buff + size, &elem->den, sizeof(elem->den));
    size += sizeof(elem->den);

    return size;
}

static int qemu_deserialize_rational(const uint8_t *buff, AVRational *elem)
{
    int size = 0;

    memset(elem, 0, sizeof(*elem));

    memcpy(&elem->num, buff + size, sizeof(elem->num));
    size += sizeof(elem->num);
    memcpy(&elem->den, buff + size, sizeof(elem->den));
    size += sizeof(elem->den);

    return size;
}

static int qemu_serialize_frame(const AVFrame *elem, uint8_t *buff)
{
    int size = 0;

    memcpy(buff + size, &elem->key_frame, sizeof(elem->key_frame));
    size += sizeof(elem->key_frame);
    memcpy(buff + size, &elem->pict_type, sizeof(elem->pict_type));
    size += sizeof(elem->pict_type);
    memcpy(buff + size, &elem->pts, sizeof(elem->pts));
    size += sizeof(elem->pts);
    memcpy(buff + size, &elem->coded_picture_number,
            sizeof(elem->coded_picture_number));
    size += sizeof(elem->coded_picture_number);
    memcpy(buff + size, &elem->display_picture_number,
            sizeof(elem->display_picture_number));
    size += sizeof(elem->display_picture_number);
    memcpy(buff + size, &elem->quality, sizeof(elem->quality));
    size += sizeof(elem->quality);
    memcpy(buff + size, &elem->age, sizeof(elem->age));
    size += sizeof(elem->age);
    memcpy(buff + size, &elem->reference, sizeof(elem->reference));
    size += sizeof(elem->reference);
    memcpy(buff + size, &elem->reordered_opaque,
            sizeof(elem->reordered_opaque));
    size += sizeof(elem->reordered_opaque);
    memcpy(buff + size, &elem->repeat_pict, sizeof(elem->repeat_pict));
    size += sizeof(elem->repeat_pict);
    memcpy(buff + size, &elem->interlaced_frame,
            sizeof(elem->interlaced_frame));
    size += sizeof(elem->interlaced_frame);

    return size;
}

static int qemu_deserialize_frame(const uint8_t *buff, AVFrame *elem)
{
    int size = 0;

    memset(elem, 0, sizeof(*elem));

    memcpy(&elem->linesize, buff + size, sizeof(elem->linesize));
    size += sizeof(elem->linesize);
    memcpy(&elem->key_frame, buff + size, sizeof(elem->key_frame));
    size += sizeof(elem->key_frame);
    memcpy(&elem->pict_type, buff + size, sizeof(elem->pict_type));
    size += sizeof(elem->pict_type);
    memcpy(&elem->pts, buff + size, sizeof(elem->pts));
    size += sizeof(elem->pts);
    memcpy(&elem->coded_picture_number, buff + size,
            sizeof(elem->coded_picture_number));
    size += sizeof(elem->coded_picture_number);
    memcpy(&elem->display_picture_number, buff + size,
            sizeof(elem->display_picture_number));
    size += sizeof(elem->display_picture_number);
    memcpy(&elem->quality, buff + size, sizeof(elem->quality));
    size += sizeof(elem->quality);
    memcpy(&elem->age, buff + size, sizeof(elem->age));
    size += sizeof(elem->age);
    memcpy(&elem->reference, buff + size, sizeof(elem->reference));
    size += sizeof(elem->reference);
    memcpy(&elem->qstride, buff + size, sizeof(elem->qstride));
    size += sizeof(elem->qstride);
    memcpy(&elem->motion_subsample_log2, buff + size,
            sizeof(elem->motion_subsample_log2));
    size += sizeof(elem->motion_subsample_log2);
    memcpy(&elem->error, buff + size, sizeof(elem->error));
    size += sizeof(elem->error);
    memcpy(&elem->type, buff + size, sizeof(elem->type));
    size += sizeof(elem->type);
    memcpy(&elem->repeat_pict, buff + size, sizeof(elem->repeat_pict));
    size += sizeof(elem->repeat_pict);
    memcpy(&elem->qscale_type, buff + size, sizeof(elem->qscale_type));
    size += sizeof(elem->qscale_type);
    memcpy(&elem->interlaced_frame, buff + size,
            sizeof(elem->interlaced_frame));
    size += sizeof(elem->interlaced_frame);
    memcpy(&elem->top_field_first, buff + size, sizeof(elem->top_field_first));
    size += sizeof(elem->top_field_first);
    memcpy(&elem->palette_has_changed, buff + size,
            sizeof(elem->palette_has_changed));
    size += sizeof(elem->palette_has_changed);
    memcpy(&elem->buffer_hints, buff + size, sizeof(elem->buffer_hints));
    size += sizeof(elem->buffer_hints);
    memcpy(&elem->reordered_opaque, buff + size,
            sizeof(elem->reordered_opaque));
    size += sizeof(elem->reordered_opaque);

    return size;
}

void qemu_parser_init(SVCodecState *s, int ctx_index)
{
    TRACE("[%s] Enter\n", __func__);

    s->ctx_arr[ctx_index].parser_buf = NULL;
    s->ctx_arr[ctx_index].parser_use = false;

    TRACE("[%s] Leave\n", __func__);
}

void qemu_codec_close(SVCodecState *s, uint32_t value)
{
    int i;
    int ctx_index = 0;

    TRACE("[%s] Enter\n", __func__);
    qemu_mutex_lock(&s->thread_mutex);

    for (i = 0; i < CODEC_MAX_CONTEXT; i++) {
        if (s->ctx_arr[i].file_value == value) {
            ctx_index = i;
            break;
        }
    }

    TRACE("[%s] close %d context\n", __func__, ctx_index);
    s->ctx_arr[ctx_index].ctx_used = false;
    qemu_parser_init(s, ctx_index);

    qemu_mutex_unlock(&s->thread_mutex);
    TRACE("[%s] Leave\n", __func__);
}

void qemu_get_codec_ver(SVCodecState *s)
{
    char codec_ver[32];
    off_t offset;
    int ctx_index;

    offset = s->codec_param.mmap_offset;
    ctx_index = s->codec_param.ctx_index;

    memset(codec_ver, 0, 32);
    pstrcpy(codec_ver, sizeof(codec_ver), MARU_CODEC_VERSION);

    TRACE("%d of codec_version:%s\n", ctx_index, codec_ver);
    memcpy((uint8_t *)s->vaddr + offset, codec_ver, sizeof(codec_ver));
}

/* void av_register_all() */
void qemu_av_register_all(void)
{
    av_register_all();
    TRACE("av_register_all\n");
}

#if 0
/* int avcodec_default_get_buffer(AVCodecContext *s, AVFrame *pic) */
int qemu_avcodec_get_buffer(AVCodecContext *context, AVFrame *picture)
{
    int ret;
    TRACE("avcodec_default_get_buffer\n");

    picture->reordered_opaque = context->reordered_opaque;
    picture->opaque = NULL;

    ret = avcodec_default_get_buffer(context, picture);

    return ret;
}

/* void avcodec_default_release_buffer(AVCodecContext *ctx, AVFrame *frame) */
void qemu_avcodec_release_buffer(AVCodecContext *context, AVFrame *picture)
{
    TRACE("avcodec_default_release_buffer\n");
    avcodec_default_release_buffer(context, picture);
}
#endif

/* int avcodec_open(AVCodecContext *avctx, AVCodec *codec) */
int qemu_avcodec_open(SVCodecState *s, int ctx_index)
{
    AVCodecContext *avctx;
    AVCodec *codec;
    enum CodecID codec_id;
    off_t offset;
    int ret = -1;
    int bEncode = 0;
    int size = 0;

    qemu_mutex_lock(&s->thread_mutex);

    avctx = s->ctx_arr[ctx_index].avctx;
    if (!avctx) {
        ERR("[%s] %d of AVCodecContext is NULL!\n", __func__, ctx_index);
        qemu_mutex_unlock(&s->thread_mutex);
        return ret;
    }

    offset = s->codec_param.mmap_offset;

    TRACE("[%s] Context Index:%d, offset:%d\n", __func__, ctx_index, offset);
    memcpy(&avctx->bit_rate, (uint8_t *)s->vaddr + offset, sizeof(int));
    size = sizeof(int);
    memcpy(&avctx->bit_rate_tolerance,
            (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    memcpy(&avctx->flags, (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    size += qemu_deserialize_rational((uint8_t *)s->vaddr + offset + size,
                                        &avctx->time_base);
    memcpy(&avctx->width, (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    memcpy(&avctx->height, (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    memcpy(&avctx->gop_size, (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    memcpy(&avctx->pix_fmt, (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    memcpy(&avctx->sample_rate,
            (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    memcpy(&avctx->channels, (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    memcpy(&avctx->codec_tag, (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    memcpy(&avctx->block_align,
            (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    memcpy(&avctx->rc_strategy,
            (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    memcpy(&avctx->strict_std_compliance,
            (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    memcpy(&avctx->rc_qsquish,
            (uint8_t *)s->vaddr + offset + size, sizeof(float));
    size += sizeof(float);
    size += qemu_deserialize_rational((uint8_t *)s->vaddr + offset + size,
            &avctx->sample_aspect_ratio);
    memcpy(&avctx->qmin, (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    memcpy(&avctx->qmax, (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    memcpy(&avctx->pre_me, (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    memcpy(&avctx->trellis, (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    memcpy(&avctx->extradata_size,
            (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    memcpy(&codec_id, (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    memcpy(&bEncode, (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    TRACE("Context Index:%d, width:%d, height:%d\n",
            ctx_index, avctx->width, avctx->height);

    if (avctx->extradata_size > 0) {
        avctx->extradata = (uint8_t *)av_mallocz(avctx->extradata_size +
                             MARU_ROUND_UP_16(FF_INPUT_BUFFER_PADDING_SIZE));
        memcpy(avctx->extradata,
                (uint8_t *)s->vaddr + offset + size, avctx->extradata_size);
        size += avctx->extradata_size;
    } else {
        TRACE("[%s] allocate dummy extradata\n", __func__);
        avctx->extradata =
                av_mallocz(MARU_ROUND_UP_16(FF_INPUT_BUFFER_PADDING_SIZE));
    }

    if (bEncode) {
        TRACE("[%s] find encoder :%d\n", __func__, codec_id);
        codec = avcodec_find_encoder(codec_id);
    } else {
        TRACE("[%s] find decoder :%d\n", __func__, codec_id);
        codec = avcodec_find_decoder(codec_id);
    }

    if (!codec) {
        ERR("[%s] failed to find codec of %d\n", __func__, codec_id);
    }

#if 0
    avctx->get_buffer = qemu_avcodec_get_buffer;
    avctx->release_buffer = qemu_avcodec_release_buffer;
#endif

    ret = avcodec_open(avctx, codec);
    if (ret != 0) {
        ERR("[%s] avcodec_open failure, %d\n", __func__, ret);
    }

    if (avctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        TRACE("[%s] sample_rate:%d, channels:%d\n", __func__,
              avctx->sample_rate, avctx->channels);
    }

    memcpy((uint8_t *)s->vaddr + offset, &avctx->pix_fmt, sizeof(int));
    size = sizeof(int);
    size += qemu_serialize_rational(&avctx->time_base,
            (uint8_t *)s->vaddr + offset + size);
    memcpy((uint8_t *)s->vaddr + offset + size, &avctx->channels, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size,
            &avctx->sample_fmt, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size,
            &avctx->codec_type, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size, &avctx->codec_id, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size,
            &avctx->coded_width, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size,
            &avctx->coded_height, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size,
            &avctx->ticks_per_frame, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size,
            &avctx->chroma_sample_location, sizeof(int));
    size += sizeof(int);
#if 0
    memcpy((uint8_t *)s->vaddr + offset + size,
            avctx->priv_data, codec->priv_data_size);
    size += codec->priv_data_size;
#endif
    memcpy((uint8_t *)s->vaddr + offset + size, &ret, sizeof(int));
    size += sizeof(int);

    qemu_mutex_unlock(&s->thread_mutex);
    TRACE("Leave, %s\n", __func__);
    return ret;
}

/* int avcodec_close(AVCodecContext *avctx) */
int qemu_avcodec_close(SVCodecState *s, int ctx_index)
{
    AVCodecContext *avctx;
    off_t offset;
    int ret = -1;

    TRACE("Enter, %s\n", __func__);
    qemu_mutex_lock(&s->thread_mutex);

    offset = s->codec_param.mmap_offset;

    avctx = s->ctx_arr[ctx_index].avctx;
    if (!avctx) {
        ERR("[%s] %d of AVCodecContext is NULL\n", __func__, ctx_index);
        memcpy((uint8_t *)s->vaddr + offset, &ret, sizeof(int));
        qemu_mutex_unlock(&s->thread_mutex);
        return ret;
    }

    ret = avcodec_close(avctx);
    TRACE("after avcodec_close. ret:%d\n", ret);

    memcpy((uint8_t *)s->vaddr + offset, &ret, sizeof(int));

    qemu_mutex_unlock(&s->thread_mutex);

    TRACE("[%s] Leave\n", __func__);
    return ret;
}

/* AVCodecContext* avcodec_alloc_context (void) */
void qemu_avcodec_alloc_context(SVCodecState *s)
{
    off_t offset;
    int index;

    TRACE("[%s] Enter\n", __func__);
    qemu_mutex_lock(&s->thread_mutex);

    offset = s->codec_param.mmap_offset;

    for (index = 0; index < CODEC_MAX_CONTEXT; index++) {
        if (s->ctx_arr[index].ctx_used == false) {
            TRACE("[%s] Succeeded to get context[%d].\n", __func__, index);
            break;
        }
        TRACE("[%s] Failed to get context[%d].\n", __func__, index);
    }

    if (index == CODEC_MAX_CONTEXT) {
        ERR("[%s] Failed to get available codec context from now\n", __func__);
        ERR("[%s] Try to run codec again\n", __func__);
        qemu_mutex_unlock(&s->thread_mutex);
        return;
    }

    TRACE("allocate %d of context and frame.\n", index);
    s->ctx_arr[index].avctx = avcodec_alloc_context();
    s->ctx_arr[index].frame = avcodec_alloc_frame();

    s->ctx_arr[index].file_value = s->codec_param.file_index;
    s->ctx_arr[index].ctx_used = true;
    memcpy((uint8_t *)s->vaddr + offset, &index, sizeof(int));
    qemu_parser_init(s, index);

    qemu_mutex_unlock(&s->thread_mutex);

    TRACE("[%s] Leave\n", __func__);
}

#if 0
/* AVFrame *avcodec_alloc_frame(void) */
void qemu_avcodec_alloc_frame(SVCodecState *s, int ctx_index)
{
    AVFrame *frame = NULL;

    TRACE("[%s] Enter\n", __func__);
    qemu_mutex_lock(&s->thread_mutex);

    frame = avcodec_alloc_frame();
    if (!frame) {
        ERR("[%s] Failed to allocate frame\n", __func__);
    }
    s->ctx_arr[ctx_index].frame = frame;

    qemu_mutex_unlock(&s->thread_mutex);
    TRACE("[%s] Leave\n", __func__);
}
#endif

/* void av_free(void *ptr) */
void qemu_av_free(SVCodecState *s, int ctx_index)
{
    AVCodecContext *avctx;
    AVFrame *avframe;

    TRACE("enter %s\n", __func__);
    qemu_mutex_lock(&s->thread_mutex);

    avctx = s->ctx_arr[ctx_index].avctx;
    avframe = s->ctx_arr[ctx_index].frame;

    if (avctx && avctx->palctrl) {
        av_free(avctx->palctrl);
        avctx->palctrl = NULL;
    }

    if (avctx && avctx->extradata) {
        TRACE("free extradata\n");
        av_free(avctx->extradata);
        avctx->extradata = NULL;
    }

    if (avctx) {
        TRACE("free codec context\n");
        av_free(avctx);
        s->ctx_arr[ctx_index].avctx = NULL;
    }

    if (avframe) {
        TRACE("free codec frame\n");
        av_free(avframe);
        s->ctx_arr[ctx_index].frame = NULL;
    }

    qemu_mutex_unlock(&s->thread_mutex);
    TRACE("leave %s\n", __func__);
}

/* void avcodec_flush_buffers(AVCodecContext *avctx) */
void qemu_avcodec_flush_buffers(SVCodecState *s, int ctx_index)
{
    AVCodecContext *avctx;

    TRACE("Enter\n");
    qemu_mutex_lock(&s->thread_mutex);

    avctx = s->ctx_arr[ctx_index].avctx;
    if (avctx) {
        avcodec_flush_buffers(avctx);
    } else {
        ERR("[%s] %d of AVCodecContext is NULL\n", __func__, ctx_index);
    }

    qemu_mutex_unlock(&s->thread_mutex);
    TRACE("[%s] Leave\n", __func__);
}

/* int avcodec_decode_video(AVCodecContext *avctx, AVFrame *picture,
 *                          int *got_picture_ptr, const uint8_t *buf,
 *                          int buf_size)
 */
int qemu_avcodec_decode_video(SVCodecState *s, int ctx_index)
{
    AVCodecContext *avctx;
    AVFrame *picture;
    AVPacket avpkt;
    int got_picture_ptr;
    uint8_t *buf;
    uint8_t *parser_buf;
    bool parser_use;
    int buf_size;
    int size = 0;
    int ret = -1;
    off_t offset;

    TRACE("Enter, %s\n", __func__);
    qemu_mutex_lock(&s->thread_mutex);

    TRACE("[%s] Video Context Index : %d\n", __func__, ctx_index);

    avctx = s->ctx_arr[ctx_index].avctx;
    picture = s->ctx_arr[ctx_index].frame;
    if (!avctx || !picture) {
        ERR("[%s] %d of Context or Frame is NULL!\n", __func__, ctx_index);
        qemu_mutex_unlock(&s->thread_mutex);
        return ret;
    }

    offset = s->codec_param.mmap_offset;

    parser_buf = s->ctx_arr[ctx_index].parser_buf;
    parser_use = s->ctx_arr[ctx_index].parser_use;
    TRACE("[%s] Parser Buffer : %p, Parser:%d\n", __func__,
            parser_buf, parser_use);

    memcpy(&avctx->reordered_opaque,
            (uint8_t *)s->vaddr + offset, sizeof(int64_t));
    size = sizeof(int64_t);
    memcpy(&avctx->skip_frame,
            (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);
    memcpy(&buf_size, (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);

    picture->reordered_opaque = avctx->reordered_opaque;

    if (parser_buf && parser_use) {
        buf = parser_buf;
    } else if (buf_size > 0) {
        TRACE("[%s] not use parser, codec_id:%x\n", __func__, avctx->codec_id);
/*      buf = av_mallocz(buf_size);
        memcpy(buf, (uint8_t *)s->vaddr + offset + size, buf_size); */
        buf = (uint8_t *)s->vaddr + offset + size;
        size += buf_size;
    } else {
        TRACE("There is no input buffer\n");
        buf = NULL;
    }

    memset(&avpkt, 0, sizeof(AVPacket));
    avpkt.data = buf;
    avpkt.size = buf_size;
    TRACE("packet buf:%p, size:%d\n", buf, buf_size);

    ret = avcodec_decode_video2(avctx, picture, &got_picture_ptr, &avpkt);

    TRACE("[%s] after decoding video, ret:%d\n", __func__, ret);
    if (ret < 0) {
        ERR("[%s] failed to decode video!!, ret:%d\n", __func__, ret);
    } else {
        if (ret == 0) {
            TRACE("[%s] no frame. packet size:%d\n", __func__, avpkt.size);
        }
        TRACE("decoded frame number:%d\n", avctx->frame_number);
    }

    memcpy((uint8_t *)s->vaddr + offset, &avctx->pix_fmt, sizeof(int));
    size = sizeof(int);
#if 0
    memcpy((uint8_t *)s->vaddr + offset + size,
            &avctx->time_base, sizeof(AVRational));
    size += sizeof(AVRational);
#endif
    size += qemu_serialize_rational(&avctx->time_base,
                                    (uint8_t *)s->vaddr + offset + size);
    memcpy((uint8_t *)s->vaddr + offset + size, &avctx->width, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size, &avctx->height, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size,
            &avctx->has_b_frames, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size,
            &avctx->frame_number, sizeof(int));
    size += sizeof(int);
#if 0
    memcpy((uint8_t *)s->vaddr + offset + size,
            &avctx->sample_aspect_ratio, sizeof(AVRational));
    size += sizeof(AVRational);
#endif
    size += qemu_serialize_rational(&avctx->sample_aspect_ratio,
                                    (uint8_t *)s->vaddr + offset + size);
    memcpy((uint8_t *)s->vaddr + offset + size,
            &avctx->internal_buffer_count, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size, &avctx->profile, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size, &avctx->level, sizeof(int));
    size += sizeof(int);
    size += qemu_serialize_frame(picture, (uint8_t *)s->vaddr + offset + size);

    memcpy((uint8_t *)s->vaddr + offset + size, &got_picture_ptr, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size, &ret, sizeof(int));
    size += sizeof(int);

#if 0
    memcpy((uint8_t *)s->vaddr + offset + size, dst.data[0], numbytes);
    av_free(buffer);

/*    av_free(buf); */

    if (parser_buf && parser_use) {
        TRACE("[%s] Free input buffer after decoding video\n", __func__);
        TRACE("[%s] input buffer : %p, %p\n",
            __func__, avpkt.data, parser_buf);
        av_free(avpkt.data);
        s->ctx_arr[ctx_index].parser_buf = NULL;
    }
#endif

    qemu_mutex_unlock(&s->thread_mutex);
    TRACE("Leave, %s\n", __func__);

    return ret;
}

/* int avcodec_encode_video(AVCodecContext *avctx, uint8_t *buf,
 *                          int buf_size, const AVFrame *pict)
 */
int qemu_avcodec_encode_video(SVCodecState *s, int ctx_index)
{
    AVCodecContext *avctx = NULL;
    AVFrame *pict = NULL;
    uint8_t *inputBuf = NULL;
    int outbufSize = 0;
    int numBytes = 0;
    int bPict = -1;
    int size = 0;
    int ret = -1;
    off_t offset;

    TRACE("Enter, %s\n", __func__);
    qemu_mutex_lock(&s->thread_mutex);

    avctx = s->ctx_arr[ctx_index].avctx;
    pict = s->ctx_arr[ctx_index].frame;
    if (!avctx || !pict) {
        ERR("[%s] %d of Context or Frame is NULL\n", __func__, ctx_index);
        qemu_mutex_unlock(&s->thread_mutex);
        return ret;
    }

    offset = s->codec_param.mmap_offset;

    size = sizeof(int);
    memcpy(&bPict, (uint8_t *)s->vaddr + offset, size);
    TRACE("[%s] avframe is :%d\n", __func__, bPict);

    if (bPict == 0) {
        memcpy(&outbufSize, (uint8_t *)s->vaddr + offset + size, size);
        size += sizeof(int);
        size +=
            qemu_deserialize_frame((uint8_t *)s->vaddr + offset + size, pict);

        numBytes =
            avpicture_get_size(avctx->pix_fmt, avctx->width, avctx->height);
        TRACE("[%s] input buffer size :%d\n", __func__, numBytes);

        inputBuf = (uint8_t *)s->vaddr + offset + size;
        if (!inputBuf) {
            ERR("[%s] failed to get input buffer\n", __func__);
            return ret;
        }
        ret = avpicture_fill((AVPicture *)pict, inputBuf, avctx->pix_fmt,
                            avctx->width, avctx->height);
        if (ret < 0) {
            ERR("after avpicture_fill, ret:%d\n", ret);
        }
        TRACE("before encode video, ticks_per_frame:%d, pts:%lld\n",
               avctx->ticks_per_frame, pict->pts);
    } else {
        TRACE("flush encoded buffers\n");
        pict = NULL;
    }

    ret = avcodec_encode_video(avctx, (uint8_t *)s->vaddr + offset,
                                outbufSize, pict);
    TRACE("encode video, ret:%d, pts:%lld, outbuf size:%d\n",
            ret, pict->pts, outbufSize);

    if (ret < 0) {
        ERR("failed to encode video.\n");
    }

    memcpy((uint8_t *)s->vaddr + offset + outbufSize, &ret, sizeof(int));

    qemu_mutex_unlock(&s->thread_mutex);
    TRACE("Leave, %s\n", __func__);

    return ret;
}

/*
 *  int avcodec_decode_audio3(AVCodecContext *avctx, int16_t *samples,
 *                            int *frame_size_ptr, AVPacket *avpkt)
 */
int qemu_avcodec_decode_audio(SVCodecState *s, int ctx_index)
{
    AVCodecContext *avctx;
    AVPacket avpkt;
    int16_t *samples;
    int frame_size_ptr;
    uint8_t *buf;
    uint8_t *parser_buf;
    bool parser_use;
    int buf_size, outbuf_size;
    int size;
    int ret = -1;
    off_t offset;

    TRACE("Enter, %s\n", __func__);
    qemu_mutex_lock(&s->thread_mutex);

    TRACE("Audio Context Index : %d\n", ctx_index);
    avctx = s->ctx_arr[ctx_index].avctx;
    if (!avctx) {
        ERR("[%s] %d of Context is NULL!\n", __func__, ctx_index);
        qemu_mutex_unlock(&s->thread_mutex);
        return ret;
    }

    if (!avctx->codec) {
        ERR("[%s] %d of Codec is NULL\n", __func__, ctx_index);
        qemu_mutex_unlock(&s->thread_mutex);
        return ret;
    }

    offset = s->codec_param.mmap_offset;

    parser_buf = s->ctx_arr[ctx_index].parser_buf;
    parser_use = s->ctx_arr[ctx_index].parser_use;

    memcpy(&buf_size, (uint8_t *)s->vaddr + offset, sizeof(int));
    size = sizeof(int);
    TRACE("input buffer size : %d\n", buf_size);

    if (parser_buf && parser_use) {
        TRACE("[%s] use parser, buf:%p codec_id:%x\n",
                __func__, parser_buf, avctx->codec_id);
        buf = parser_buf;
    } else if (buf_size > 0) {
        TRACE("[%s] not use parser, codec_id:%x\n", __func__, avctx->codec_id);
        buf = (uint8_t *)s->vaddr + offset + size;
        size += buf_size;
    } else {
        TRACE("no input buffer\n");
        buf = NULL;
    }

    av_init_packet(&avpkt);
    avpkt.data = buf;
    avpkt.size = buf_size;

    frame_size_ptr = AVCODEC_MAX_AUDIO_FRAME_SIZE;
    outbuf_size = frame_size_ptr;
    samples = av_malloc(frame_size_ptr);

    ret = avcodec_decode_audio3(avctx, samples, &frame_size_ptr, &avpkt);
    TRACE("after decoding audio!, ret:%d\n", ret);

    memcpy((uint8_t *)s->vaddr + offset, &avctx->bit_rate, sizeof(int));
    size = sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size, &avctx->sample_rate, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size, &avctx->channels, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size, &avctx->sub_id, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size,
            &avctx->frame_size, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size,
            &avctx->frame_number, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size, samples, outbuf_size);
    size += outbuf_size;
    memcpy((uint8_t *)s->vaddr + offset + size, &frame_size_ptr, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size, &ret, sizeof(int));
    size += sizeof(int);

    TRACE("before free input buffer and output buffer!\n");
    if (samples) {
        av_free(samples);
        samples = NULL;
    }

    if (parser_buf && parser_use) {
        TRACE("[%s] free parser buf\n", __func__);
        av_free(avpkt.data);
        s->ctx_arr[ctx_index].parser_buf = NULL;
    }

    qemu_mutex_unlock(&s->thread_mutex);
    TRACE("[%s] Leave\n", __func__);

    return ret;
}

int qemu_avcodec_encode_audio(SVCodecState *s, int ctx_index)
{
    WARN("[%s] Does not support audio encoder using FFmpeg\n", __func__);
    return 0;
}

/* void av_picture_copy(AVPicture *dst, const AVPicture *src,
 *                      enum PixelFormat pix_fmt, int width, int height)
 */
void qemu_av_picture_copy(SVCodecState *s, int ctx_index)
{
    AVCodecContext *avctx;
    AVPicture dst;
    AVPicture *src;
    int numBytes;
    int ret;
    uint8_t *buffer = NULL;
    off_t offset;

    TRACE("Enter :%s\n", __func__);
    qemu_mutex_lock(&s->thread_mutex);

    avctx = s->ctx_arr[ctx_index].avctx;
    src = (AVPicture *)s->ctx_arr[ctx_index].frame;
    if (!avctx && !src) {
        ERR("[%s] %d of context or frame is NULL\n", __func__, ctx_index);
        qemu_mutex_unlock(&s->thread_mutex);
        return;
    }

    offset = s->codec_param.mmap_offset;

    numBytes =
        avpicture_get_size(avctx->pix_fmt, avctx->width, avctx->height);
    if (numBytes < 0) {
        ERR("[%s] failed to get size of pixel format:%d\n",
            __func__, avctx->pix_fmt);
    }
    TRACE("After avpicture_get_size:%d\n", numBytes);

    buffer = (uint8_t *)av_mallocz(numBytes);
    if (!buffer) {
        ERR("[%s] failed to allocate memory\n");
        qemu_mutex_unlock(&s->thread_mutex);
        return;
    }

    ret = avpicture_fill(&dst, buffer, avctx->pix_fmt,
                        avctx->width, avctx->height);
    av_picture_copy(&dst, src, avctx->pix_fmt, avctx->width, avctx->height);
    memcpy((uint8_t *)s->vaddr + offset, dst.data[0], numBytes);
    TRACE("after copy image buffer from host to guest.\n");

    if (buffer) {
        av_free(buffer);
    }

    qemu_mutex_unlock(&s->thread_mutex);
    TRACE("Leave :%s\n", __func__);
}

/* AVCodecParserContext *av_parser_init(int codec_id) */
void qemu_av_parser_init(SVCodecState *s, int ctx_index)
{
    AVCodecParserContext *parser_ctx = NULL;
    AVCodecContext *avctx;

    TRACE("Enter :%s\n", __func__);
    qemu_mutex_lock(&s->thread_mutex);

    avctx = s->ctx_arr[ctx_index].avctx;
    if (!avctx) {
        ERR("[%s] %d of AVCodecContext is NULL!!\n", __func__, ctx_index);
        qemu_mutex_unlock(&s->thread_mutex);
        return;
    }

    TRACE("before av_parser_init, codec_type:%d codec_id:%x\n",
            avctx->codec_type, avctx->codec_id);

    parser_ctx = av_parser_init(avctx->codec_id);
    if (parser_ctx) {
        TRACE("[%s] using parser\n", __func__);
        s->ctx_arr[ctx_index].parser_use = true;
    } else {
        TRACE("[%s] no parser\n", __func__);
        s->ctx_arr[ctx_index].parser_use = false;
    }
    s->ctx_arr[ctx_index].parser_ctx = parser_ctx;

    qemu_mutex_unlock(&s->thread_mutex);
    TRACE("[%s] Leave\n", __func__);
}

/* int av_parser_parse(AVCodecParserContext *s, AVCodecContext *avctx,
 *                     uint8_t **poutbuf, int *poutbuf_size,
 *                     const uint8_t *buf, int buf_size,
 *                     int64_t pts, int64_t dts)
 */
int qemu_av_parser_parse(SVCodecState *s, int ctx_index)
{
    AVCodecParserContext *parser_ctx = NULL;
    AVCodecContext *avctx = NULL;
    uint8_t *poutbuf;
    int poutbuf_size = 0;
    uint8_t *inbuf = NULL;
    int inbuf_size;
    int64_t pts;
    int64_t dts;
    int64_t pos;
    int size, ret = -1;
    off_t offset;

    TRACE("Enter %s\n", __func__);
    qemu_mutex_lock(&s->thread_mutex);

    parser_ctx = s->ctx_arr[ctx_index].parser_ctx;
    avctx = s->ctx_arr[ctx_index].avctx;
    if (!avctx) {
        ERR("[%s] %d of AVCodecContext is NULL\n", __func__, ctx_index);
        qemu_mutex_unlock(&s->thread_mutex);
        return ret;
    }

    if (!parser_ctx) {
        ERR("[%s] %d of AVCodecParserContext is NULL\n", __func__, ctx_index);
        qemu_mutex_unlock(&s->thread_mutex);
        return ret;
    }

    offset = s->codec_param.mmap_offset;

    memcpy(&parser_ctx->pts,
        (uint8_t *)s->vaddr + offset, sizeof(int64_t));
    size = sizeof(int64_t);
    memcpy(&parser_ctx->dts,
        (uint8_t *)s->vaddr + offset + size, sizeof(int64_t));
    size += sizeof(int64_t);
    memcpy(&parser_ctx->pos,
        (uint8_t *)s->vaddr + offset + size, sizeof(int64_t));
    size += sizeof(int64_t);
    memcpy(&pts, (uint8_t *)s->vaddr + offset + size, sizeof(int64_t));
    size += sizeof(int64_t);
    memcpy(&dts, (uint8_t *)s->vaddr + offset + size, sizeof(int64_t));
    size += sizeof(int64_t);
    memcpy(&pos, (uint8_t *)s->vaddr + offset + size, sizeof(int64_t));
    size += sizeof(int64_t);
    memcpy(&inbuf_size, (uint8_t *)s->vaddr + offset + size, sizeof(int));
    size += sizeof(int);

    if (inbuf_size > 0) {
        inbuf = av_mallocz(inbuf_size);
        memcpy(inbuf, (uint8_t *)s->vaddr + offset + size, inbuf_size);
    } else {
        inbuf = NULL;
        INFO("[%s] input buffer size is zero.\n", __func__);
    }

    TRACE("[%s] inbuf:%p inbuf_size :%d\n", __func__, inbuf, inbuf_size);
    ret = av_parser_parse2(parser_ctx, avctx, &poutbuf, &poutbuf_size,
                           inbuf, inbuf_size, pts, dts, pos);
    TRACE("after parsing, outbuf size :%d, ret:%d\n", poutbuf_size, ret);

    if (poutbuf) {
        s->ctx_arr[ctx_index].parser_buf = poutbuf;
    }

    TRACE("[%s] inbuf : %p, outbuf : %p\n", __func__, inbuf, poutbuf);
    memcpy((uint8_t *)s->vaddr + offset, &parser_ctx->pts, sizeof(int64_t));
    size = sizeof(int64_t);
    memcpy((uint8_t *)s->vaddr + offset + size, &poutbuf_size, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t *)s->vaddr + offset + size, &ret, sizeof(int));
    size += sizeof(int);
    if (poutbuf && poutbuf_size > 0) {
        memcpy((uint8_t *)s->vaddr + offset + size, poutbuf, poutbuf_size);
    } else {
        av_free(inbuf);
    }

#if 0
    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        TRACE("[%s] free parser inbuf\n", __func__);
        av_free(inbuf);
    }
#endif

    qemu_mutex_unlock(&s->thread_mutex);
    TRACE("Leave, %s\n", __func__);

    return ret;
}

/* void av_parser_close(AVCodecParserContext *s) */
void qemu_av_parser_close(SVCodecState *s, int ctx_index)
{
    AVCodecParserContext *parser_ctx;

    TRACE("Enter, %s\n", __func__);
    qemu_mutex_lock(&s->thread_mutex);

    parser_ctx = s->ctx_arr[ctx_index].parser_ctx;
    if (!parser_ctx) {
        ERR("AVCodecParserContext is NULL\n");
        qemu_mutex_unlock(&s->thread_mutex);
        return;
    }
    av_parser_close(parser_ctx);

    qemu_mutex_unlock(&s->thread_mutex);
    TRACE("Leave, %s\n", __func__);
}

int codec_operate(uint32_t api_index, uint32_t ctx_index, SVCodecState *s)
{
    int ret = -1;

    TRACE("[%s] context : %d\n", __func__, ctx_index);
    switch (api_index) {
    /* FFMPEG API */
    case EMUL_AV_REGISTER_ALL:
        qemu_av_register_all();
        break;
    case EMUL_AVCODEC_OPEN:
        ret = qemu_avcodec_open(s, ctx_index);
        break;
    case EMUL_AVCODEC_CLOSE:
        ret = qemu_avcodec_close(s, ctx_index);
        break;
    case EMUL_AVCODEC_ALLOC_CONTEXT:
        qemu_avcodec_alloc_context(s);
        break;
#if 0
    case EMUL_AVCODEC_ALLOC_FRAME:
        qemu_avcodec_alloc_frame(s, ctx_index);
        break;
#endif
    case EMUL_AV_FREE:
        qemu_av_free(s, ctx_index);
        break;
    case EMUL_AVCODEC_FLUSH_BUFFERS:
        qemu_avcodec_flush_buffers(s, ctx_index);
        break;
#ifndef CODEC_THREAD
    case EMUL_AVCODEC_DECODE_VIDEO:
    case EMUL_AVCODEC_ENCODE_VIDEO:
    case EMUL_AVCODEC_DECODE_AUDIO:
    case EMUL_AVCODEC_ENCODE_AUDIO:
        wake_codec_worker_thread(s);
        break;
#else
    case EMUL_AVCODEC_DECODE_VIDEO:
        ret = qemu_avcodec_decode_video(s, ctx_index);
        break;
    case EMUL_AVCODEC_ENCODE_VIDEO:
        ret = qemu_avcodec_encode_video(s, ctx_index);
        break;
    case EMUL_AVCODEC_DECODE_AUDIO:
        ret = qemu_avcodec_decode_audio(s, ctx_index);
        break;
    case EMUL_AVCODEC_ENCODE_AUDIO:
        ret = qemu_avcodec_encode_audio(s, ctx_index);
        break;
#endif
    case EMUL_AV_PICTURE_COPY:
        qemu_av_picture_copy(s, ctx_index);
        break;
    case EMUL_AV_PARSER_INIT:
        qemu_av_parser_init(s, ctx_index);
        break;
    case EMUL_AV_PARSER_PARSE:
        ret = qemu_av_parser_parse(s, ctx_index);
        break;
    case EMUL_AV_PARSER_CLOSE:
        qemu_av_parser_close(s, ctx_index);
        break;
    case EMUL_GET_CODEC_VER:
        qemu_get_codec_ver(s);
        break;
    default:
        WARN("api index %d does not exist!.\n", api_index);
    }
    return ret;
}

/*
 *  Codec Device APIs
 */
uint64_t codec_read(void *opaque, target_phys_addr_t addr, unsigned size)
{
    SVCodecState *s = (SVCodecState *)opaque;
    uint64_t ret = 0;

    switch (addr) {
    case CODEC_QUERY_STATE:
        ret = s->thread_state;
        s->thread_state = 0;
        TRACE("thread state: %d\n", s->thread_state);
        qemu_irq_lower(s->dev.irq[0]);
        break;
    default:
        ERR("There is no avaiable command for %s\n", MARU_CODEC_DEV_NAME);
    }

    return ret;
}

void codec_write(void *opaque, target_phys_addr_t addr,
                uint64_t value, unsigned size)
{
    int ret = -1;
    SVCodecState *s = (SVCodecState *)opaque;

/*  qemu_mutex_lock(&s->thread_mutex); */
    switch (addr) {
    case CODEC_API_INDEX:
        ret = codec_operate(value, s->codec_param.ctx_index, s);
        break;
    case CODEC_CONTEXT_INDEX:
        s->codec_param.ctx_index = value;
        TRACE("Context Index: %d\n", s->codec_param.ctx_index);
        break;
    case CODEC_MMAP_OFFSET:
        s->codec_param.mmap_offset = value;
        TRACE("MMAP Offset: %d\n", s->codec_param.mmap_offset);
        break;
    case CODEC_FILE_INDEX:
        s->codec_param.file_index = value;
        break;
    case CODEC_CLOSED:
        qemu_codec_close(s, value);
        break;
    default:
        ERR("There is no avaiable command for %s\n", MARU_CODEC_DEV_NAME);
    }
/*  qemu_mutex_unlock(&s->thread_mutex); */
}

static const MemoryRegionOps codec_mmio_ops = {
    .read = codec_read,
    .write = codec_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void codec_tx_bh(void *opaque)
{
    SVCodecState *s = (SVCodecState *)opaque;

    int ctx_index;
    AVCodecContext *ctx;

    ctx_index = s->codec_param.ctx_index;
    ctx = s->ctx_arr[ctx_index].avctx;

    TRACE("Enter, %s\n", __func__);

    /* raise irq as soon as a worker thread had finished a job*/
    if (s->thread_state) {
        TRACE("raise codec irq. state:%d, codec:%d\n",
                s->thread_state, ctx->codec_type);
        qemu_irq_raise(s->dev.irq[0]);
/*      s->thread_state = 0;
        qemu_bh_schedule(s->tx_bh); */
    }

    TRACE("Leave, %s\n", __func__);
}

static int codec_initfn(PCIDevice *dev)
{
    SVCodecState *s = DO_UPCAST(SVCodecState, dev, dev);
    uint8_t *pci_conf = s->dev.config;

    INFO("[%s] device init\n", __func__);

    memset(&s->codec_param, 0, sizeof(SVCodecParam));
/*    pthread_mutex_init(&s->codec_mutex, NULL); */
#ifndef CODEC_THREAD
    qemu_mutex_init(&s->thread_mutex);
    qemu_cond_init(&s->thread_cond);

    codec_thread_init(s);

    s->tx_bh = qemu_bh_new(codec_tx_bh, s);
#endif

    pci_config_set_interrupt_pin(pci_conf, 1);

    memory_region_init_ram(&s->vram, NULL, "codec.ram", MARU_CODEC_MEM_SIZE);
    s->vaddr = memory_region_get_ram_ptr(&s->vram);

    memory_region_init_io(&s->mmio, &codec_mmio_ops, s,
                        "codec-mmio", MARU_CODEC_REG_SIZE);

    pci_register_bar(&s->dev, 0, PCI_BASE_ADDRESS_MEM_PREFETCH, &s->vram);
    pci_register_bar(&s->dev, 1, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->mmio);

    return 0;
}

static int codec_exitfn(PCIDevice *dev)
{
    SVCodecState *s = DO_UPCAST(SVCodecState, dev, dev);
    INFO("[%s] device exit\n", __func__);

    qemu_bh_delete(s->tx_bh);

    memory_region_destroy(&s->vram);
    memory_region_destroy(&s->mmio);
    return 0;
}

int codec_init(PCIBus *bus)
{
    INFO("[%s] device create\n", __func__);
    pci_create_simple(bus, -1, MARU_CODEC_DEV_NAME);
    return 0;
}

static PCIDeviceInfo codec_info = {
    .qdev.name      = MARU_CODEC_DEV_NAME,
    .qdev.desc      = "Virtual Codec device for Tizen emulator",
    .qdev.size      = sizeof(SVCodecState),
    .init           = codec_initfn,
    .exit           = codec_exitfn,
    .vendor_id      = PCI_VENDOR_ID_TIZEN,
    .device_id      = PCI_DEVICE_ID_VIRTUAL_CODEC,
    .class_id       = PCI_CLASS_MULTIMEDIA_AUDIO,
};

static void codec_register(void)
{
    pci_qdev_register(&codec_info);
}
device_init(codec_register);
