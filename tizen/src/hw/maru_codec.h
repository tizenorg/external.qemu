/*
 * Virtual Codec device
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Contact:
 *  Kitae Kim <kt920.kim@samsung.com>
 *  SeokYeon Hwang <syeon.hwang@samsung.com>
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

#include <stdio.h>
#include <sys/types.h>
#include "hw.h"
#include "kvm.h"
#include "pci.h"
#include "pci_ids.h"
#include "qemu-thread.h"
#include "tizen/src/debug_ch.h"
#include "maru_device_ids.h"

#include <libavformat/avformat.h>

#define CODEC_MAX_CONTEXT   1024
#define CODEC_COPY_DATA

/*
 *  Codec Device Structures
 */
typedef struct _SVCodecParam {
    uint32_t        api_index;
    uint32_t        ctx_index;
    uint32_t        file_index;
    uint32_t        mmap_offset;
} SVCodecParam;

typedef struct _SVCodecContext {
    AVCodecContext          *avctx;
    AVFrame                 *frame;
    AVCodecParserContext    *parser_ctx;
    uint8_t                 *parser_buf;
    bool                    parser_use;
    bool                    ctx_used;
    uint32_t                file_value;
} SVCodecContext;

typedef struct _SVCodecState {
    PCIDevice           dev;
    SVCodecContext      ctx_arr[CODEC_MAX_CONTEXT];
    SVCodecParam        codec_param;

    uint8_t             *vaddr;
    MemoryRegion        vram;
    MemoryRegion        mmio;

    QEMUBH              *tx_bh;
    QemuThread          thread_id;
    QemuMutex           thread_mutex;
    QemuCond            thread_cond;
    uint8_t             thread_state;
} SVCodecState;

enum {
    CODEC_API_INDEX         = 0x00,
    CODEC_QUERY_STATE       = 0x04,
    CODEC_CONTEXT_INDEX     = 0x08,
    CODEC_MMAP_OFFSET       = 0x0c,
    CODEC_FILE_INDEX        = 0x10,
    CODEC_CLOSED            = 0x14,
};

enum {
    EMUL_AV_REGISTER_ALL = 1,
    EMUL_AVCODEC_ALLOC_CONTEXT,
    EMUL_AVCODEC_OPEN,
    EMUL_AVCODEC_CLOSE,
    EMUL_AV_FREE,
    EMUL_AVCODEC_FLUSH_BUFFERS,
    EMUL_AVCODEC_DECODE_VIDEO,
    EMUL_AVCODEC_ENCODE_VIDEO,
    EMUL_AVCODEC_DECODE_AUDIO,
    EMUL_AVCODEC_ENCODE_AUDIO,
    EMUL_AV_PICTURE_COPY,
    EMUL_AV_PARSER_INIT,
    EMUL_AV_PARSER_PARSE,
    EMUL_AV_PARSER_CLOSE,
    EMUL_GET_CODEC_VER = 50,
};


/*
 *  Codec Thread Functions
 */
void codec_thread_init(SVCodecState *s);
void codec_thread_exit(SVCodecState *s);
void *codec_worker_thread(void *opaque);
void wake_codec_worker_thread(SVCodecState *s);
int decode_codec(SVCodecState *s);
int encode_codec(SVCodecState *s);

/*
 *  Codec Device Functions
 */
int codec_init(PCIBus *bus);
uint64_t codec_read(void *opaque, target_phys_addr_t addr,
                    unsigned size);
void codec_write(void *opaque, target_phys_addr_t addr,
                uint64_t value, unsigned size);
int codec_operate(uint32_t api_index, uint32_t ctx_index,
                SVCodecState *state);

/*
 *  Codec Helper Functions
 */
void qemu_parser_init(SVCodecState *s, int ctx_index);
void qemu_codec_close(SVCodecState *s, uint32_t value);
void qemu_get_codec_ver(SVCodecState *s);

/*
 *  FFMPEG Functions
 */
void qemu_av_register_all(void);
int qemu_avcodec_open(SVCodecState *s, int ctx_index);
int qemu_avcodec_close(SVCodecState *s, int ctx_index);
void qemu_avcodec_alloc_context(SVCodecState *s);
void qemu_avcodec_flush_buffers(SVCodecState *s, int ctx_index);
int qemu_avcodec_decode_video(SVCodecState *s, int ctx_index);
int qemu_avcodec_encode_video(SVCodecState *s, int ctx_index);
int qemu_avcodec_decode_audio(SVCodecState *s, int ctx_index);
int qemu_avcodec_encode_audio(SVCodecState *s, int ctx_index);
void qemu_av_picture_copy(SVCodecState *s, int ctx_index);
void qemu_av_parser_init(SVCodecState *s, int ctx_index);
int qemu_av_parser_parse(SVCodecState *s, int ctx_index);
void qemu_av_parser_close(SVCodecState *s, int ctx_index);
int qemu_avcodec_get_buffer(AVCodecContext *context, AVFrame *picture);
void qemu_avcodec_release_buffer(AVCodecContext *context, AVFrame *picture);
void qemu_av_free(SVCodecState *s, int ctx_index);
