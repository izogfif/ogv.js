// #define ORIG_IMPL

#ifdef ORIG_IMPL
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#include <ogg/ogg.h>

#include <theora/theoradec.h>

#include "ogv-decoder-video.h"
#include "ogv-ogg-support.h"

// Video decode state
th_info           theoraInfo;
th_comment        theoraComment;
th_setup_info    *theoraSetupInfo;
th_dec_ctx       *theoraDecoderContext;

int               theoraHeaders = 0;
int               theoraProcessingHeaders;

static int display_width = 0;
static int display_height = 0;

void ogv_video_decoder_init(void) {
    // init supporting Theora structures needed in header parsing
    th_comment_init(&theoraComment);
    th_info_init(&theoraInfo);
}

int ogv_video_decoder_async(void) {
  return 0;
}

int ogv_video_decoder_process_frame(const char *data, size_t data_len);

int ogv_video_decoder_process_header(const char *data, size_t data_len) {
	ogg_packet oggPacket;
	ogv_ogg_import_packet(&oggPacket, data, data_len);

	// hack, theora looks for packet b_o_s
	if (theoraHeaders == 0) {
		oggPacket.b_o_s = 256;
	}
	theoraHeaders++;

	theoraProcessingHeaders = th_decode_headerin(&theoraInfo, &theoraComment, &theoraSetupInfo, &oggPacket);
	if (theoraProcessingHeaders == 0) {
		// We've completed the theora header
		theoraDecoderContext = th_decode_alloc(&theoraInfo, theoraSetupInfo);

		int hdec = !(theoraInfo.pixel_fmt & 1);
		int vdec = !(theoraInfo.pixel_fmt & 2);

		display_width = theoraInfo.pic_width;
		display_height = theoraInfo.pic_height;
		if (theoraInfo.aspect_numerator > 0 && theoraInfo.aspect_denominator > 0) {
			display_width = display_width * theoraInfo.aspect_numerator / theoraInfo.aspect_denominator;
		}
		ogvjs_callback_init_video(theoraInfo.frame_width, theoraInfo.frame_height,
		                          theoraInfo.frame_width >> hdec, theoraInfo.frame_height >> vdec,
		                          0.0f, // don't expose fixed fps; we pretend it's variable to handle dupe frames more cleanly
		                          theoraInfo.pic_width, theoraInfo.pic_height,
		                          theoraInfo.pic_x, theoraInfo.pic_y,
		                          display_width, display_height);
		
		// Last header packet is also first data packet.
		return ogv_video_decoder_process_frame(data, data_len);
	} else if (theoraProcessingHeaders > 0) {
		return 1;
	} else {
		//printf("Error parsing theora headers: %d.\n", theoraProcessingHeaders);
		return 0;
	}
}

int ogv_video_decoder_process_frame(const char *data, size_t data_len) {
	ogg_packet oggPacket;
	ogv_ogg_import_packet(&oggPacket, data, data_len);

    int ret = th_decode_packetin(theoraDecoderContext, &oggPacket, NULL);
    if (ret == 0 || ret == TH_DUPFRAME) {
		th_ycbcr_buffer ycbcr;
		th_decode_ycbcr_out(theoraDecoderContext, ycbcr);

		int hdec = !(theoraInfo.pixel_fmt & 1);
		int vdec = !(theoraInfo.pixel_fmt & 2);

		ogvjs_callback_frame(ycbcr[0].data, ycbcr[0].stride,
							 ycbcr[1].data, ycbcr[1].stride,
							 ycbcr[2].data, ycbcr[2].stride,
							 theoraInfo.frame_width, theoraInfo.frame_height,
							 theoraInfo.frame_width >> hdec, theoraInfo.frame_height >> vdec,
							 theoraInfo.pic_width, theoraInfo.pic_height,
							 theoraInfo.pic_x, theoraInfo.pic_y,
							 display_width, display_height);
        return 1;
    } else {
        //printf("Theora decoder failed mysteriously? %d\n", ret);
        return 0;
    }
}

void ogv_video_decoder_destroy(void) {
    if (theoraDecoderContext) {
        th_decode_free(theoraDecoderContext);
        theoraDecoderContext = NULL;
    }
	th_comment_clear(&theoraComment);
	th_info_clear(&theoraInfo);
}
#endif

#ifndef ORIG_IMPL
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdio.h>

#include "ogv-decoder-video.h"
#include "ogv-ogg-support.h"

static int display_width = 0;
static int display_height = 0;

void ogv_video_decoder_init(void)
{
}

int ogv_video_decoder_async(void)
{
 return 0;
}

int ogv_video_decoder_process_header(const char *data, size_t data_len)
{
 printf("ogv-decoder-video-theora: ogv_video_decoder_process_header is being called. data=%p, data size=%lld\n", data, data_len);
 return 1;
}

int read_int(const char **pBuf)
{
 int result = -1;
 memcpy(&result, *pBuf, 4);
 *pBuf += 4;
	return result;
}

int ogv_video_decoder_process_frame(const char *data, size_t data_len)
{
 // printf("ogv-decoder-video-theora: ogv_video_decoder_process_frame is being called. data size=%d\n", data_len);
 if (!data_len)
 {
		return 1;
 }
 char *pBuf = data;
 int width = read_int(&pBuf);
 int height = read_int(&pBuf);
 int linesize0 = read_int(&pBuf);
 int linesize1 = read_int(&pBuf);
 int linesize2 = read_int(&pBuf);
 const int datasize0 = linesize0 * height;
 const int datasize1 = linesize1 * height / 2;
 const int datasize2 = linesize2 * height / 2;
 printf("ogv-decoder-video-theora: width=%d, height=%d, \
	 linesize0=%d, linesize1=%d, linesize2=%d, \
		datasize0=%d, datasize1=%d, datasize2=%d\n",
								width, height,
								linesize0, linesize1, linesize2,
								datasize0, datasize1, datasize2);

 ogvjs_callback_frame(
					pBuf, linesize0,
					pBuf + datasize0, linesize1,
					pBuf + datasize0 + datasize1, linesize2,
					width, height,
					width / 2, height / 2,
					width, height,
					0, 0,
					width, height);
 return 1;
}

void ogv_video_decoder_destroy(void)
{
}
#endif