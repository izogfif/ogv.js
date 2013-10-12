#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <theora/codec.h>
#include <theora/theoradec.h>


/* never forget that globals are a one-way ticket to Hell */
/* Ogg and codec state for demux/decode */
ogg_sync_state    oggSyncState;
ogg_page          oggPage;

/* Video decode state */
ogg_stream_state  theoraStreamState;
th_info           theoraInfo;
th_comment        theoraComment;
th_setup_info    *theoraSetupInfo;
th_dec_ctx       *theoraDecoderContext;

int              theora_p=0;
int              theora_processing_headers;

/* single frame video buffering */
int          videobuf_ready=0;
ogg_int64_t  videobuf_granulepos=-1;
double       videobuf_time=0;
int          raw=0;

/* Audio decode state */
int              vorbis_p = 0;
ogg_stream_state vo;
vorbis_info      vi;
vorbis_dsp_state vd;
vorbis_block     vb;
vorbis_comment   vc;
int          crop=0;

int got_sigint=0;
static void sigint_handler (int signal) {
  got_sigint = 1;
}

extern void OgvJsOutputFrame(unsigned char *bufferY, int strideY,
                             unsigned char *bufferCb, int strideCb,
                             unsigned char *bufferCr, int strideCr,
                             int width, int height,
                             int hdec, int vdec);

/*Write out the planar YUV frame, uncropped.*/
static void video_write(void){
    th_ycbcr_buffer ycbcr;
    th_decode_ycbcr_out(theoraDecoderContext,ycbcr);

    int hdec = !(theoraInfo.pixel_fmt & 1);
    int vdec = !(theoraInfo.pixel_fmt & 2);
    
	OgvJsOutputFrame(ycbcr[0].data, ycbcr[0].stride,
	                 ycbcr[1].data, ycbcr[1].stride,
	                 ycbcr[2].data, ycbcr[2].stride,
	                 theoraInfo.frame_width, theoraInfo.frame_height,
	                 hdec, vdec);
}

/* dump the theora comment header */
static int dump_comments(th_comment *_tc){
  int   i;
  int   len;
  printf("Encoded by %s\n",_tc->vendor);
  if(_tc->comments){
    printf("theora comment header:\n");
    for(i=0;i<_tc->comments;i++){
      if(_tc->user_comments[i]){
        len=_tc->comment_lengths[i]<INT_MAX?_tc->comment_lengths[i]:INT_MAX;
        printf("\t%.*s\n",len,_tc->user_comments[i]);
      }
    }
  }
  return 0;
}

/* helper: push a page into the appropriate steam */
/* this can be done blindly; a stream won't accept a page
                that doesn't belong to it */
static int queue_page(ogg_page *page){
  if (theora_p) ogg_stream_pagein(&theoraStreamState,page);
  if (vorbis_p) ogg_stream_pagein(&vo, page);
  return 0;
}


  ogg_packet oggPacket;

  int frames = 0;

enum AppState {
	STATE_BEGIN,
	STATE_HEADERS,
	STATE_DECODING
} appState;

void OgvJsInit() {
	appState = STATE_BEGIN;
	
  /* start up Ogg stream synchronization layer */
  ogg_sync_init(&oggSyncState);

  /* init supporting Vorbis structures needed in header parsing */
  vorbis_info_init(&vi);
  vorbis_comment_init(&vc);

  /* init supporting Theora structures needed in header parsing */
  th_comment_init(&theoraComment);
  th_info_init(&theoraInfo);
}

static void processHeaders();

static void processBegin() {
	if (ogg_page_bos(&oggPage)) {
		printf("Packet is at start of a bitstream\n");
		int got_packet;
	
		// Initialize a stream state object...
		ogg_stream_state test;
		ogg_stream_init(&test,ogg_page_serialno(&oggPage));
		ogg_stream_pagein(&test, &oggPage);
		ogg_stream_packetout(&test, &oggPacket);
		
		/* identify the codec: try theora */
		if(!theora_p && (theora_processing_headers = th_decode_headerin(&theoraInfo,&theoraComment,&theoraSetupInfo,&oggPacket))>=0){

			/* it is theora -- save this stream state */
			printf("found theora stream!\n");
			memcpy(&theoraStreamState, &test, sizeof(test));
			theora_p=1;
		} else if (!vorbis_p && (vorbis_synthesis_headerin(&vi,&vc,&oggPacket)>=0)) {
			// it's vorbis!
			// save this as our audio stream...
			printf("found vorbis stream!\n");
			memcpy(&vo, &test, sizeof(test));
			vorbis_p = 1;
		} else {
			printf("already have stream, or not theora or vorbis packet\n");
			/* whatever it is, we don't care about it */
			ogg_stream_clear(&test);
		}
	} else {
		// Not a bitstream start -- move on to header decoding...
		appState = STATE_HEADERS;
		processHeaders();
	}
}

static void processHeaders() {
	queue_page(&oggPage);
	if ((theora_p && theora_p < 3) || (vorbis_p && vorbis_p < 3)) {
		int ret;

		/* look for further theora headers */
		while (theora_p && (theora_p < 3)) {
			ret = ogg_stream_packetout(&theoraStreamState, &oggPacket);
			if (ret < 0) {
				printf("Error reading theora headers.\n");
				exit(1);
			}
			ret = th_decode_headerin(&theoraInfo, &theoraComment, &theoraSetupInfo, &oggPacket);
		    if (ret == 0) {
		  	  // We've completed the theora header
		      printf("Completed theora header.\n");
		  	  theora_p = 3;
		  	  break;
		    }
		    theora_p++;
		}
		
		while (vorbis_p && (vorbis_p < 3) && (ogg_stream_packetout(&vo, &oggPacket) < 0)) {
			if (vorbis_synthesis_headerin(&vi, &vc, &oggPacket)) {
				printf("Error parsing Vorbis stream headers; corrupt stream?\n");
				exit(1);
			}
			vorbis_p++;
			if (vorbis_p == 3) {
				printf("Vorbis headers done.\n");
				break;
			}
		}		

		/* The header pages/packets will arrive before anything else we
		   care about, or the stream is not obeying spec */

		/*
		if(ogg_sync_pageout(&oggSyncState,&oggPage)>0){
		    queue_page(&oggPage); // demux into the appropriate stream
		} else {
			// We ran out of input; return and wait for more.
			printf("Ran out of input while searching for codec headers.\n");
			return;
		}
		*/
    } else {
	  /* and now we have it all.  initialize decoders */
	  printf("theora_p is %d; vorbis_p is %d\n", theora_p, vorbis_p);
	  if(theora_p){
	  printf("SETTING UP THEORA DECODER CONTEXT\n");
		dump_comments(&theoraComment);
		theoraDecoderContext=th_decode_alloc(&theoraInfo,theoraSetupInfo);
		printf("Ogg logical stream %lx is Theora %dx%d %.02f fps video\n"
		 "Encoded frame content is %dx%d with %dx%d offset\n",
		 theoraStreamState.serialno,theoraInfo.frame_width,theoraInfo.frame_height,
		 (double)theoraInfo.fps_numerator/theoraInfo.fps_denominator,
		 theoraInfo.pic_width,theoraInfo.pic_height,theoraInfo.pic_x,theoraInfo.pic_y);
	  }

		if (vorbis_p) {
			vorbis_synthesis_init(&vd,&vi);
			vorbis_block_init(&vd,&vb);
		}

		  appState = STATE_DECODING;
		  printf("Done with headers step\n");
	}
}

static void processDecoding() {
	queue_page(&oggPage);
	if (theora_p) {
		if(!videobuf_ready){
		  /* theora is one in, one out... */
		  if (ogg_stream_packetout(&theoraStreamState, &oggPacket) > 0 ){

			if (th_decode_packetin(theoraDecoderContext,&oggPacket,&videobuf_granulepos)>=0){
			  videobuf_time=th_granule_time(theoraDecoderContext,videobuf_granulepos);
			  videobuf_ready=1;
			  frames++;
			}

		  }
		}

		if(videobuf_ready){
			/* dumpvideo frame, and get new one */
			video_write();
			videobuf_ready=0;
		}
	}
	
	if (vorbis_p) {
		if (ogg_stream_packetout(&vo, &oggPacket) > 0) {
			printf("ignoring audio packet\n");
		}
	}
}

void OgvJsReceiveInput(char *buffer, int bufsize) {
	if (bufsize > 0) {
		char *dest = ogg_sync_buffer(&oggSyncState, bufsize);
		memcpy(dest, buffer, bufsize);
		ogg_sync_wrote(&oggSyncState, bufsize);
	}
}

int OgvJsProcess() {
	if (ogg_sync_pageout(&oggSyncState, &oggPage) > 0) {
		if (appState == STATE_BEGIN) {
			processBegin();
		} else if (appState == STATE_HEADERS) {
			processHeaders();
		} else if (appState == STATE_DECODING) {
			processDecoding();
		}
		return 1;
	}
	return 0;
}


void OgvJsDestroy() {
  if(theora_p){
    ogg_stream_clear(&theoraStreamState);
    th_decode_free(theoraDecoderContext);
    th_comment_clear(&theoraComment);
    th_info_clear(&theoraInfo);
  }
  ogg_sync_clear(&oggSyncState);
}
