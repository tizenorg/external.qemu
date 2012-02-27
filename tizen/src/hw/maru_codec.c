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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Contributors:
 * - S-Core Co., Ltd
 *
 */

#include "maru_codec.h"

#define QEMU_DEV_NAME           "codec"
#define SVCODEC_MEM_SIZE        (8 * 1024 * 1024)
#define SVCODEC_REG_SIZE        (256)

/* define debug channel */
MULTI_DEBUG_CHANNEL(qemu, svcodec);

static AVCodecContext *gAVCtx = NULL;
static AVFrame *gFrame = NULL;
static AVCodecParserContext *gAVParserCtx = NULL;
static uint8_t* gParserOutBuf = NULL;
static bool bParser; 
static int paramCount = 0;

void qemu_parser_init (void)
{
    gParserOutBuf = NULL;
    bParser = false;
}

void qemu_restore_context (AVCodecContext *dst, AVCodecContext *src) {
    dst->av_class = src->av_class;
    dst->extradata = src->extradata;
    dst->codec = src->codec;
    dst->priv_data = src->priv_data;
    dst->opaque = src->opaque;
    dst->get_buffer = src->get_buffer;
    dst->release_buffer = src->release_buffer;
    dst->stats_out = src->stats_out;
    dst->stats_in = src->stats_in;
    dst->rc_override = src->rc_override;
    dst->rc_eq = src->rc_eq;
    dst->slice_offset = src->slice_offset;
    dst->get_format = src->get_format;
    dst->internal_buffer = src->internal_buffer;
    dst->intra_matrix = src->intra_matrix;
    dst->inter_matrix = src->inter_matrix;
    dst->reget_buffer = src->reget_buffer;
    dst->execute = src->execute;
    dst->thread_opaque = src->thread_opaque;
    dst->execute2 = src->execute2;
}

/* void av_register_all() */
void qemu_av_register_all (void)
{
    av_register_all();
}

/* int avcodec_default_get_buffer (AVCodecContext *s, AVFrame *pic) */
int qemu_avcodec_get_buffer (AVCodecContext *context, AVFrame *picture)
{
    int ret;
    TRACE("avcodec_default_get_buffer\n");

    picture->reordered_opaque = context->reordered_opaque;
    picture->opaque = NULL;

    ret = avcodec_default_get_buffer(context, picture);

    return ret;
}

/* void avcodec_default_release_buffer (AVCodecContext *ctx, AVFrame *frame) */
void qemu_avcodec_release_buffer (AVCodecContext *context, AVFrame *picture)
{
    TRACE("avcodec_default_release_buffer\n");
 
    avcodec_default_release_buffer(context, picture);
}

/* int avcodec_open (AVCodecContext *avctx, AVCodec *codec) */
#ifdef CODEC_HOST
int qemu_avcodec_open (SVCodecState *s)
{
    AVCodecContext *avctx;
    AVCodecContext tmpCtx;
    AVCodec *codec;
    AVCodec tmpCodec;
    enum CodecID codec_id;
    int ret, size;

    if (!gAVCtx) {
        ERR("AVCodecContext is NULL!!\n");
        return -1;
    }
    avctx = gAVCtx;

    size = sizeof(AVCodecContext);
    memcpy(&tmpCtx, avctx, size);

    cpu_synchronize_state(cpu_single_env);

    cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[0],
                        (uint8_t*)avctx, sizeof(AVCodecContext), 0);
    cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[1],
                        (uint8_t*)&tmpCodec, sizeof(AVCodec), 0);

    /* restore AVCodecContext's pointer variables */
    qemu_restore_context(avctx, &tmpCtx);
    codec_id = tmpCodec.id;

    if (avctx->extradata_size > 0) {
        avctx->extradata = (uint8_t*)av_malloc(avctx->extradata_size);
        cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[2],
                            (uint8_t*)avctx->extradata, avctx->extradata_size, 0);
    } else {
        avctx->extradata = NULL;
    }

    TRACE("[%s][%d] CODEC ID : %d\n", __func__, __LINE__, codec_id);    
    if (tmpCodec.encode) {
        codec = avcodec_find_encoder(codec_id);
    } else {
        codec = avcodec_find_decoder(codec_id);
    }

    avctx->get_buffer = qemu_avcodec_get_buffer;
    avctx->release_buffer = qemu_avcodec_release_buffer;

    ret = avcodec_open(avctx, codec);
    if (ret != 0) {
        perror("Failed to open codec\n");
        return ret;
    }

    cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[0],
                        (uint8_t*)avctx, sizeof(AVCodecContext), 1);

    return ret;
}
#else
int qemu_avcodec_open (SVCodecState *s)
{
    AVCodecContext *avctx;
    AVCodecContext tempCtx;
    AVCodec *codec;
    AVCodec tmpCodec;
    enum CodecID codec_id;
    int ret;
    int size;

    if (!gAVCtx) {
        ERR("AVCodecContext is NULL!!\n");
        return -1;
    }

    avctx = gAVCtx;
    size = sizeof(AVCodecContext);
    memcpy(&tempCtx, avctx, size);

    memcpy(avctx, s->vaddr, size);
    memcpy(&tmpCodec, (uint8_t*)s->vaddr + size, sizeof(AVCodec));

    /* restore AVCodecContext's pointer variables */
    qemu_restore_context(avctx, &tempCtx);

    codec_id = tmpCodec.id;
    size += sizeof(AVCodec);

    if (avctx->extradata_size > 0) {
        avctx->extradata = (uint8_t*)av_malloc(avctx->extradata_size);
        memcpy(avctx->extradata, (uint8_t*)s->vaddr + size, avctx->extradata_size);
    } else {
        avctx->extradata = NULL;
    }

    if (tmpCodec.encode) {
        codec = avcodec_find_encoder(codec_id);
    } else {
        codec = avcodec_find_decoder(codec_id);
    }

    avctx->get_buffer = qemu_avcodec_get_buffer;
    avctx->release_buffer = qemu_avcodec_release_buffer;
    
    ret = avcodec_open(avctx, codec);
    if (ret != 0) {
        ERR("Failed to open codec, %d\n", ret);
    }

    memcpy(s->vaddr, avctx, sizeof(AVCodecContext));
    memcpy((uint8_t*)s->vaddr + sizeof(AVCodecContext), &ret, sizeof(int));

    return ret;
}
#endif

/* int avcodec_close (AVCodecContext *avctx) */
int qemu_avcodec_close (SVCodecState* s)
{
    AVCodecContext *avctx;
    int ret = -1;
    
    avctx = gAVCtx;
    if (!avctx) {
        ERR("[%s][%d] AVCodecContext is NULL\n", __func__, __LINE__);
        memcpy(s->vaddr, &ret, sizeof(int));
        return ret;
    }

    ret = avcodec_close(avctx);
    TRACE("after avcodec_close. ret:%d\n", ret);

#ifndef CODEC_HOST
    memcpy(s->vaddr, &ret, sizeof(int));
#endif

//  qemu_parser_init();

    return ret;
}

/* AVCodecContext* avcodec_alloc_context (void) */
void qemu_avcodec_alloc_context (void)
{
    gAVCtx = avcodec_alloc_context();
    qemu_parser_init();
}

/* AVFrame *avcodec_alloc_frame (void) */
void qemu_avcodec_alloc_frame (void)
{
    gFrame = avcodec_alloc_frame();
}

/* void av_free (void *ptr) */
void qemu_av_free_context (void)
{
    av_free(gAVCtx);
    gAVCtx = NULL;
    TRACE("free AVCodecContext\n");
}

void qemu_av_free_picture (void)
{
    if (gFrame) {
        av_free(gFrame);
        gFrame = NULL;
    }
    TRACE("free AVFrame\n");
}

void qemu_av_free_palctrl (void)
{
    if (gAVCtx->palctrl) {
        av_free(gAVCtx->palctrl);
        gAVCtx->palctrl = NULL;
    }
    TRACE("free AVCodecContext palctrl\n");
}

void qemu_av_free_extradata (void)
{
    if (gAVCtx && gAVCtx->extradata && gAVCtx->extradata_size > 0) {
        av_free(gAVCtx->extradata);
        gAVCtx->extradata = NULL;
    }
    TRACE("free AVCodecContext extradata\n");
}

/* void avcodec_flush_buffers (AVCodecContext *avctx) */
void qemu_avcodec_flush_buffers (void)
{
    AVCodecContext *avctx;

    TRACE("Enter\n");

    avctx = gAVCtx;
    if (avctx) {
        avcodec_flush_buffers(avctx);
    } else {
        ERR("AVCodecContext is NULL\n");
    }
    TRACE("Leave\n");
}

/* int avcodec_decode_video (AVCodecContext *avctx, AVFrame *picture,
 *                          int *got_picture_ptr, const uint8_t *buf,
 *                          int buf_size)
 */
#ifdef CODEC_HOST
int qemu_avcodec_decode_video (SVCodecState* s)
{
    AVCodecContext *avctx = NULL;
    AVFrame *picture = NULL;
    int got_picture_ptr = 0;
    const uint8_t *buf = NULL;
    int buf_size = 0;
    int ret;

    avctx = gAVCtx;
    picture = gFrame;
    if (avctx == NULL || picture == NULL) {
        ERR("AVCodecContext or AVFrame is NULL");
        return -1;
    }
    
    cpu_synchronize_state(cpu_single_env);
    cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[4],
                        (uint8_t*)&buf_size, sizeof(int), 0);

    TRACE("Paser Buffer : 0x%x, Parser:%d\n", gParserOutBuf, bParser);
    if (gParserOutBuf && bParser) {
        buf = gParserOutBuf;
    } else if (buf_size > 0) {
        buf = (uint8_t*)av_malloc(buf_size * sizeof(uint8_t));
        cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[3],
                            (uint8_t*)buf, buf_size, 0);
    } else {
        TRACE("There is no input buffer\n");
    }

    TRACE("before avcodec_decode_video\n");

    ret = avcodec_decode_video(avctx, picture, &got_picture_ptr, buf, buf_size);

    TRACE("after avcodec_decode_video, ret:%d\n", ret);
    if (got_picture_ptr == 0) {
        TRACE("There is no frame\n");
    }

    if (!gParserOutBuf && !bParser) {
        TRACE("not use parser, codec_id:%d\n", avctx->codec_id);
        av_free(buf);
    }

    cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[0],
                        (uint8_t*)avctx, sizeof(AVCodecContext), 1);
    cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[1],
                        (uint8_t*)picture, sizeof(AVFrame), 1);
    cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[2],
                        (uint8_t*)&got_picture_ptr, sizeof(int), 1);

    return ret;
}
#else
int qemu_avcodec_decode_video (SVCodecState *s)
{
    AVCodecContext *avctx; 
    AVFrame *picture;
    int got_picture_ptr;
    uint8_t *buf;
    int buf_size;
    int size;
    int ret;

    avctx = gAVCtx;
    picture = gFrame;
    if (avctx == NULL || picture == NULL) {
        ERR("AVCodecContext or AVFrame is NULL!! avctx:0x%x, picture:0x%x\n", avctx, picture);
        return -1;
    }

    memcpy(&buf_size, (uint8_t*)s->vaddr, sizeof(int));
    size = sizeof(int);

    TRACE("Paser Buffer : 0x%x, Parser:%d\n", gParserOutBuf, bParser);
    if (gParserOutBuf && bParser) {
        buf = gParserOutBuf;
    } else if (buf_size > 0) {
        buf = (uint8_t*)s->vaddr + size;
    } else {
        TRACE("There is no input buffer\n");
    }
    
    TRACE("before avcodec_decode_video\n");
    ret = avcodec_decode_video(avctx, picture, &got_picture_ptr, buf, buf_size);
    TRACE("after avcodec_decode_video, ret:%d\n", ret);
    if (got_picture_ptr == 0) {
        TRACE("There is no frame\n");
    }

    size = sizeof(AVCodecContext);
    memcpy(s->vaddr, avctx, size);
    memcpy((uint8_t*)s->vaddr + size, picture, sizeof(AVFrame));
    size += sizeof(AVFrame);
    memcpy((uint8_t*)s->vaddr + size, &got_picture_ptr, sizeof(int));
    size += sizeof(int);
    memcpy((uint8_t*)s->vaddr + size, &ret, sizeof(int));

    if (!gParserOutBuf && !bParser) {
        TRACE("not use parser, codec_id:%d\n", avctx->codec_id);
    }

    return ret;
}
#endif

/* int avcodec_encode_video (AVCodecContext *avctx, uint8_t *buf,
 *                          int buf_size, const AVFrame *pict)
 */
#ifdef CODEC_HOST
int qemu_avcodec_encode_video (SVCodecState *s)
{
    AVCodecContext *avctx;
    uint8_t *outBuf, *inBuf;
    int outBufSize, inBufSize;
    AVFrame *pict;
    int ret;

    if (gAVCtx) {
        avctx = gAVCtx;
        pict = gFrame;
    } else {
        ERR("AVCodecContext is NULL\n");
        return -1;
    }

    cpu_synchronize_state(cpu_single_env);

    cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[2],
                        (uint8_t*)&outBufSize, sizeof(int), 0);

    if (outBufSize > 0) {
        outBuf = (uint8_t*)av_malloc(outBufSize * sizeof(uint8_t));
        cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[1],
                            (uint8_t*)outBuf, outBufSize, 0);
    } else {
        ERR("input buffer size is 0\n");
        return -1;
    }

    cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[3],
                        (uint8_t*)pict, sizeof(AVFrame), 0);
    inBufSize = avpicture_get_size(avctx->pix_fmt, avctx->width, avctx->height);
    inBuf = (uint8_t*)av_malloc(inBufSize * sizeof(uint8_t));
    cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[4],
                        (uint8_t*)inBuf, inBufSize, 0);
    
    ret = avpicture_fill((AVPicture*)pict, inBuf, avctx->pix_fmt,
                        avctx->width, avctx->height);
    if (ret < 0) {
        ERR("after avpicture_fill, ret:%d\n", ret);
    }

    ret = avcodec_encode_video (avctx, outBuf, outBufSize, pict);
    TRACE("after avcodec_encode_video, ret:%d\n", ret);

    if (ret > 0) {
        cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[5],
                            (uint8_t*)avctx->coded_frame, sizeof(AVFrame), 1);
        cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[6],
                            (uint8_t*)outBuf, outBufSize, 1);
    }
    av_free(inBuf);
    return ret;
}
#else
int qemu_avcodec_encode_video (SVCodecState *s)
{
    AVCodecContext *avctx = NULL;
    AVFrame *pict = NULL;
    uint8_t *inputBuf = NULL;
    int outputBufSize = 0;
    int numBytes = 0;
    int bPict = -1;
    int size = 0;
    int ret = -1;

    if (gAVCtx) {
        avctx = gAVCtx;
        pict = gFrame;
    } else {
        ERR("AVCodecContext is NULL\n");
        return -1;
    }

    size = sizeof(int);
    memcpy(&bPict, s->vaddr, size);
    TRACE("avframe is :%d\n", bPict);

    if (bPict == 0) {
        memcpy(&outputBufSize, (uint8_t*)s->vaddr + size, size);
        size += sizeof(int);

        memcpy(pict, (uint8_t*)s->vaddr + size, sizeof(AVFrame));
        size += sizeof(AVFrame);

        numBytes = avpicture_get_size(avctx->pix_fmt, avctx->width, avctx->height);
        TRACE("input buffer size :%d\n", numBytes);

        inputBuf = (uint8_t*)s->vaddr + size;
        if (!inputBuf) {
            ERR("failed to allocate decoded frame inputBuf\n");
            return -1;
        }

        ret = avpicture_fill((AVPicture*)pict, inputBuf, avctx->pix_fmt,
                avctx->width, avctx->height);
        if (ret < 0) {
            ERR("after avpicture_fill, ret:%d\n", ret);
        }

        TRACE("before encode video, ticks_per_frame:%d, pts:%d\n",
                avctx->ticks_per_frame, pict->pts);
    } else {
        TRACE("Flush encoded buffers\n");
        pict = NULL;
    }

    TRACE("before encoding video\n");
    ret = avcodec_encode_video (avctx, s->vaddr, outputBufSize, pict);

    TRACE("after encoding video, ret:%d\n");
    if (ret < 0) {
        ERR("Failed to encode video\n");
    }
    memcpy((uint8_t*)s->vaddr + outputBufSize, &ret, sizeof(int));

    return ret;
}
#endif

/* void av_picture_copy (AVPicture *dst, const AVPicture *src,
 *                      enum PixelFormat pix_fmt, int width, int height)
 */
#ifdef CODEC_HOST
void qemu_av_picture_copy (SVCodecState *s)
{
    AVCodecContext* avctx;
    AVPicture dst;
    AVPicture *src;
    int numBytes;
    uint8_t *buffer = NULL;
    int ret;

    TRACE("Enter :%s\n", __func__);
    if (gAVCtx && gFrame) { 
        avctx = gAVCtx;
        src = (AVPicture*)gFrame;
    } else {
        ERR("AVCodecContext or AVFrame is NULL\n");
        return;
    }

    numBytes = avpicture_get_size(avctx->pix_fmt, avctx->width, avctx->height);
    buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    avpicture_fill(&dst, buffer, avctx->pix_fmt, avctx->width, avctx->height);
    av_picture_copy(&dst, src, avctx->pix_fmt, avctx->width, avctx->height);

    cpu_synchronize_state(cpu_single_env);
    ret = cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[5],
                              (uint8_t*)dst.data[0], numBytes, 1);
    if (ret < 0) {
        TRACE("failed to copy decoded frame into guest!! ret:%d\n", ret);
    }

    av_free(buffer);

    TRACE("Leave :%s\n", __func__);
}
#else
void qemu_av_picture_copy (SVCodecState *s)
{
    AVCodecContext* avctx;
    AVPicture dst;
    AVPicture *src;
    int numBytes;
    uint8_t *buffer = NULL;

    TRACE("Enter :%s\n", __func__);
    if (gAVCtx && gFrame) { 
        avctx = gAVCtx;
        src = (AVPicture*)gFrame;
    } else {
        ERR("AVCodecContext or AVFrame is NULL\n");
        return;
    }

    numBytes = avpicture_get_size(avctx->pix_fmt, avctx->width, avctx->height);
    buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    avpicture_fill(&dst, buffer, avctx->pix_fmt, avctx->width, avctx->height);
    av_picture_copy(&dst, src, avctx->pix_fmt, avctx->width, avctx->height);

    memcpy(s->vaddr, dst.data[0], numBytes);
    TRACE("After copy image buffer from host to guest\n");
    
    av_free(buffer);
    TRACE("Leave :%s\n", __func__);
}
#endif

/* AVCodecParserContext *av_parser_init (int codec_id) */
void qemu_av_parser_init (SVCodecState *s)
{
    AVCodecParserContext *parserctx = NULL;
    AVCodecContext *avctx;

    TRACE("Enter :%s\n", __func__);

    avctx = gAVCtx;
    if (avctx == NULL) {
        ERR("AVCodecContext is NULL!!\n");
        return;
    }

    TRACE("before av_parser_init, codec_id:%d\n", avctx->codec_id);
    parserctx = av_parser_init(avctx->codec_id);
    if (!parserctx) {
        ERR("Failed to initialize AVCodecParserContext\n");
    }
    gAVParserCtx = parserctx;
    bParser = true;

    TRACE("Leave :%s\n", __func__);
}

/* int av_parser_parse(AVCodecParserContext *s, AVCodecContext *avctx,
 *                      uint8_t **poutbuf, int *poutbuf_size,
 *                      const uint8_t *buf, int buf_size,
 *                      int64_t pts, int64_t dts)
 */
#ifdef CODEC_HOST
int qemu_av_parser_parse (SVCodecState *s)
{
    AVCodecParserContext *parserctx = NULL;
    AVCodecContext *avctx = NULL;
    AVCodecContext tmp_ctx;
    uint8_t *poutbuf = NULL;
    int poutbuf_size;
    uint8_t *inbuf = NULL;
    int inbuf_size;
    int64_t pts;
    int64_t dts;
    int ret;

    if (gAVParserCtx && gAVCtx) {
        parserctx = gAVParserCtx;
        avctx = gAVCtx;
    } else {
        ERR("AVCodecParserContext or AVCodecContext is NULL\n");
    }

    memcpy(&tmp_ctx, avctx, sizeof(AVCodecContext));

    cpu_synchronize_state(cpu_single_env);
    cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[1],
                        (uint8_t*)avctx, sizeof(AVCodecContext), 0);
    qemu_restore_context(avctx, &tmp_ctx);

    cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[5],
                        (uint8_t*)&inbuf_size, sizeof(int), 0);
    if (inbuf_size > 0) {
        inbuf = av_malloc(sizeof(uint8_t) * inbuf_size);
        cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[4],
                            (uint8_t*)inbuf, inbuf_size, 0);
    }
    cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[6],
                        (uint8_t*)&pts, sizeof(int64_t), 0);
    cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[7],
                        (uint8_t*)&dts, sizeof(int64_t), 0);
    
    ret = av_parser_parse(parserctx, avctx, &poutbuf, &poutbuf_size,
                          inbuf, inbuf_size, pts, dts);
    gParserOutBuf = poutbuf;
    if (inbuf_size > 0 && inbuf) {
        av_free(inbuf);
        inbuf = NULL;
    }

    cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[0],
                        (uint8_t*)parserctx, sizeof(AVCodecParserContext), 1);
    cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[1],
                        (uint8_t*)avctx, sizeof(AVCodecContext), 1);
    cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[3],
                        (uint8_t*)&poutbuf_size, sizeof(int), 1);
    if (poutbuf_size != 0) {
        cpu_memory_rw_debug(cpu_single_env, s->codecParam.in_args[2],
                            (uint8_t*)poutbuf, poutbuf_size, 1);
    }

    TRACE("Leave %s\n", __func__);
    return ret;
}
#else
int qemu_av_parser_parse (SVCodecState *s)
{
    AVCodecParserContext *parserctx = NULL;
    AVCodecContext *avctx = NULL;
    AVCodecContext tmp_ctx;

    uint8_t *poutbuf;
    int poutbuf_size;
    uint8_t *inbuf = NULL;
    int inbuf_size;
    int64_t pts;
    int64_t dts;
    int size, ret;

    if (gAVParserCtx && gAVCtx) {
        parserctx = gAVParserCtx;
        avctx = gAVCtx;
    } else {
        ERR("AVCodecParserContext or AVCodecContext is NULL\n");
    }

    TRACE("Enter %s\n", __func__);
    memcpy(&tmp_ctx, avctx, sizeof(AVCodecContext));

    size = sizeof(AVCodecContext);
    memcpy(avctx, s->vaddr, sizeof(AVCodecContext));
    qemu_restore_context(avctx, &tmp_ctx);
    memcpy(&pts, (uint8_t*)s->vaddr + size, sizeof(int64_t));
    size += sizeof(int64_t);
    memcpy(&dts, (uint8_t*)s->vaddr + size, sizeof(int64_t));
    size += sizeof(int64_t);
    memcpy(&inbuf_size, (uint8_t*)s->vaddr + size, sizeof(int));
    if (inbuf_size > 0) {
        size += sizeof(int);
        inbuf = (uint8_t*)s->vaddr + size;
        size += inbuf_size;
    } else {
        inbuf = NULL;
    }
    
    ret = av_parser_parse(parserctx, avctx, &poutbuf, &poutbuf_size,
                          inbuf, inbuf_size, pts, dts);

    gParserOutBuf = poutbuf;

    size = sizeof(AVCodecContext);
    memcpy(s->vaddr, avctx, sizeof(AVCodecContext));

    memcpy((uint8_t*)s->vaddr + size, &poutbuf_size, sizeof(int));
    size += sizeof(int);
    if (poutbuf_size != 0) {
        memcpy((uint8_t*)s->vaddr + size, poutbuf, poutbuf_size);
        size += poutbuf_size;
    }
    memcpy((uint8_t*)s->vaddr + size, &ret, sizeof(int));

    TRACE("Leave %s\n", __func__);
    return ret;
}
#endif

/* void av_parser_close (AVCodecParserContext *s) */
void qemu_av_parser_close (void)
{
    AVCodecParserContext *parserctx;

    TRACE("av_parser_close\n");
    if (gAVParserCtx) {
        parserctx = gAVParserCtx;
    } else {
        ERR("AVCodecParserContext is NULL\n");
        return ;
    }
    av_parser_close(parserctx);
}

static int codec_operate (uint32_t apiIndex, SVCodecState *state)
{
    int ret = -1;

    switch (apiIndex) {
        /* FFMPEG API */
        case EMUL_AV_REGISTER_ALL:
            qemu_av_register_all();
            break;
        case EMUL_AVCODEC_OPEN:
            ret = qemu_avcodec_open(state);
            break;
        case EMUL_AVCODEC_CLOSE:
            ret = qemu_avcodec_close(state);
            break;
        case EMUL_AVCODEC_ALLOC_CONTEXT:
            qemu_avcodec_alloc_context();
            break;
        case EMUL_AVCODEC_ALLOC_FRAME:
            qemu_avcodec_alloc_frame();
            break;
        case EMUL_AV_FREE_CONTEXT:
            qemu_av_free_context();
            break;
        case EMUL_AV_FREE_FRAME:
            qemu_av_free_picture();
            break;
        case EMUL_AV_FREE_PALCTRL:
            qemu_av_free_palctrl();
            break;
        case EMUL_AV_FREE_EXTRADATA:
            qemu_av_free_extradata();
            break;
        case EMUL_AVCODEC_FLUSH_BUFFERS:
            qemu_avcodec_flush_buffers();
            break;
        case EMUL_AVCODEC_DECODE_VIDEO:
            ret = qemu_avcodec_decode_video(state);
            break;
        case EMUL_AVCODEC_ENCODE_VIDEO:
            ret = qemu_avcodec_encode_video(state);
            break;
        case EMUL_AV_PICTURE_COPY:
            qemu_av_picture_copy(state);
            break;
        case EMUL_AV_PARSER_INIT:
            qemu_av_parser_init(state);
            break;
        case EMUL_AV_PARSER_PARSE:
            ret = qemu_av_parser_parse(state);
            break;
        case EMUL_AV_PARSER_CLOSE:
            qemu_av_parser_close();
            break;
        default:
            WARN("The api index does not exsit!. api index:%d\n", apiIndex);
    }
    return ret;
}

/*
 *  Codec Device APIs
 */
static uint32_t codec_read (void *opaque, target_phys_addr_t addr)
{
    int ret = -1;

    switch (addr) {
        default:
            ERR("There is no avaiable command for svcodece\n");
    }
    return ret;
}

static void codec_write (void *opaque, target_phys_addr_t addr, uint32_t value)
{
    uint32_t offset;
    int ret = -1;
    SVCodecState *state = (SVCodecState*)opaque;

    offset = addr;
    switch (offset) {
        case FUNC_NUM:
            ret = codec_operate(value, state);
#ifdef CODEC_HOST
            if (ret >= 0) {
                cpu_synchronize_state(cpu_single_env);
                cpu_memory_rw_debug(cpu_single_env, state->codecParam.ret_args,
                                    (uint8_t*)&ret, sizeof(int), 1);
            }
#endif
            paramCount = 0;
            break;
        case IN_ARGS:
            state->codecParam.in_args[paramCount++] = value;
            break;
        case RET_STR:
            state->codecParam.ret_args = value;
            break;
        default:
            ERR("There is no avaiable command for svcodece\n");
    }
}

static CPUReadMemoryFunc * const svcodec_io_readfn[3] = {
    codec_read,
    codec_read,
    codec_read,
};

static CPUWriteMemoryFunc * const svcodec_io_writefn[3] = {
    codec_write,
    codec_write,
    codec_write,
};

static void codec_mem_map (PCIDevice *dev, int region_num,
                            pcibus_t addr, pcibus_t size, int type)
{
    SVCodecState *s = DO_UPCAST(SVCodecState, dev, dev);
    cpu_register_physical_memory(addr, size, s->vram_offset);
    s->mem_addr = addr;
}

static void codec_mmio_map (PCIDevice *dev, int region_num,
                            pcibus_t addr, pcibus_t size, int type)
{
    SVCodecState *s = DO_UPCAST(SVCodecState, dev, dev);
    cpu_register_physical_memory(addr, size, s->mmioIndex);
    s->mmio_addr = addr;
}

static int codec_initfn (PCIDevice *dev)
{
    SVCodecState *s = DO_UPCAST(SVCodecState, dev, dev);
    uint8_t *pci_conf = s->dev.config;

    memset(&s->codecParam, 0x00, sizeof(SVCodecParam));
    
    pci_config_set_vendor_id(pci_conf, PCI_VENDOR_ID_TIZEN);
    pci_config_set_device_id(pci_conf, PCI_DEVICE_ID_VIRTUAL_CODEC);
    pci_config_set_class(pci_conf, PCI_CLASS_MULTIMEDIA_OTHER);
    pci_config_set_interrupt_pin(pci_conf, 2);

    s->vram_offset = qemu_ram_alloc(NULL, "codec.ram", SVCODEC_MEM_SIZE);
    s->vaddr = qemu_get_ram_ptr(s->vram_offset);

    s->mmioIndex = cpu_register_io_memory(svcodec_io_readfn, svcodec_io_writefn,
                                          s, DEVICE_LITTLE_ENDIAN);

    pci_register_bar(&s->dev, 0, SVCODEC_MEM_SIZE,
                    PCI_BASE_ADDRESS_MEM_PREFETCH, codec_mem_map);
    pci_register_bar(&s->dev, 1, SVCODEC_REG_SIZE,
                    PCI_BASE_ADDRESS_SPACE_MEMORY, codec_mmio_map);

    return 0;
}

int pci_codec_init (PCIBus *bus)
{
    printf("[%s] device init\n", __func__);
    pci_create_simple (bus, -1, QEMU_DEV_NAME);
    return 0;
}

static PCIDeviceInfo codec_info = {
    .qdev.name      = QEMU_DEV_NAME,
    .qdev.desc      = "Virtual codec device for Tizen emulator",
    .qdev.size      = sizeof (SVCodecState),    
    .init           = codec_initfn,
};

static void codec_register (void)
{
    pci_qdev_register(&codec_info);
}
device_init(codec_register);
