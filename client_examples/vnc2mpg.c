/**
 * @example vnc2mpg.c
 * Simple movie writer for vnc; based on Libavformat API example from FFMPEG
 * 
 * Copyright (c) 2003 Fabrice Bellard, 2004 Johannes E. Schindelin
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.  
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.1415926535897931
#endif

#include "avformat.h"
#include <rfb/rfbclient.h>

#define STREAM_FRAME_RATE 25 /* 25 images/s */

/**************************************************************/
/* video output */

AVFrame *picture, *tmp_picture;
uint8_t *video_outbuf;
int frame_count, video_outbuf_size;

/* add a video output stream */
AVStream *add_video_stream(AVFormatContext *oc, int codec_id, int w, int h)
{
    AVCodecContext *c;
    AVStream *st;

    st = av_new_stream(oc, 0);
    if (!st) {
        fprintf(stderr, "Could not alloc stream\n");
        exit(1);
    }
   
#if LIBAVFORMAT_BUILD<4629
    c = &st->codec;
#else
    c = st->codec;
#endif
    c->codec_id = codec_id;
    c->codec_type = CODEC_TYPE_VIDEO;

    /* put sample parameters */
    c->bit_rate = 800000;
    /* resolution must be a multiple of two */
    c->width = w;  
    c->height = h;
    /* frames per second */
#if LIBAVCODEC_BUILD<4754
    c->frame_rate = STREAM_FRAME_RATE;  
    c->frame_rate_base = 1;
#else
    c->time_base.den = STREAM_FRAME_RATE;
    c->time_base.num = 1;
    c->pix_fmt = PIX_FMT_YUV420P;
#endif
    c->gop_size = 12; /* emit one intra frame every twelve frames at most */
    if (c->codec_id == CODEC_ID_MPEG2VIDEO) {
        /* just for testing, we also add B frames */
        c->max_b_frames = 2;
    }
    if (c->codec_id == CODEC_ID_MPEG1VIDEO){
        /* needed to avoid using macroblocks in which some coeffs overflow 
           this doesnt happen with normal video, it just happens here as the 
           motion of the chroma plane doesnt match the luma plane */
        c->mb_decision=2;
    }
    /* some formats want stream headers to be seperate */
    if(!strcmp(oc->oformat->name, "mp4") || !strcmp(oc->oformat->name, "mov") || !strcmp(oc->oformat->name, "3gp"))
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;
    
    return st;
}

AVFrame *alloc_picture(int pix_fmt, int width, int height)
{
    AVFrame *picture;
    uint8_t *picture_buf;
    int size;
    
    picture = avcodec_alloc_frame();
    if (!picture)
        return NULL;
    size = avpicture_get_size(pix_fmt, width, height);
    picture_buf = malloc(size);
    if (!picture_buf) {
        av_free(picture);
        return NULL;
    }
    avpicture_fill((AVPicture *)picture, picture_buf, 
                   pix_fmt, width, height);
    return picture;
}
    
void open_video(AVFormatContext *oc, AVStream *st)
{
    AVCodec *codec;
    AVCodecContext *c;

#if LIBAVFORMAT_BUILD<4629
    c = &st->codec;
#else
    c = st->codec;
#endif

    /* find the video encoder */
    codec = avcodec_find_encoder(c->codec_id);
    if (!codec) {
        fprintf(stderr, "codec not found\n");
        exit(1);
    }

    /* open the codec */
    if (avcodec_open(c, codec) < 0) {
        fprintf(stderr, "could not open codec\n");
        exit(1);
    }

    video_outbuf = NULL;
    if (!(oc->oformat->flags & AVFMT_RAWPICTURE)) {
        /* allocate output buffer */
        /* XXX: API change will be done */
        video_outbuf_size = 200000;
        video_outbuf = malloc(video_outbuf_size);
    }

    /* allocate the encoded raw picture */
    picture = alloc_picture(c->pix_fmt, c->width, c->height);
    if (!picture) {
        fprintf(stderr, "Could not allocate picture\n");
        exit(1);
    }

    /* if the output format is not RGB565, then a temporary RGB565
       picture is needed too. It is then converted to the required
       output format */
    tmp_picture = NULL;
    if (c->pix_fmt != PIX_FMT_RGB565) {
        tmp_picture = alloc_picture(PIX_FMT_RGB565, c->width, c->height);
        if (!tmp_picture) {
            fprintf(stderr, "Could not allocate temporary picture\n");
            exit(1);
        }
    }
}

void write_video_frame(AVFormatContext *oc, AVStream *st)
{
    int out_size, ret;
    AVCodecContext *c;
    AVFrame *picture_ptr;
   
#if LIBAVFORMAT_BUILD<4629
    c = &st->codec;
#else
    c = st->codec;
#endif
    
        if (c->pix_fmt != PIX_FMT_RGB565) {
            /* as we only generate a RGB565 picture, we must convert it
               to the codec pixel format if needed */
            img_convert((AVPicture *)picture, c->pix_fmt, 
                        (AVPicture *)tmp_picture, PIX_FMT_RGB565,
                        c->width, c->height);
        }
	picture_ptr = picture;

    
    if (oc->oformat->flags & AVFMT_RAWPICTURE) {
        /* raw video case. The API will change slightly in the near
           futur for that */
        AVPacket pkt;
        av_init_packet(&pkt);
        
        pkt.flags |= PKT_FLAG_KEY;
        pkt.stream_index= st->index;
        pkt.data= (uint8_t *)picture_ptr;
        pkt.size= sizeof(AVPicture);
        
        ret = av_write_frame(oc, &pkt);
    } else {
        /* encode the image */
        out_size = avcodec_encode_video(c, video_outbuf, video_outbuf_size, picture_ptr);
        /* if zero size, it means the image was buffered */
        if (out_size != 0) {
            AVPacket pkt;
            av_init_packet(&pkt);
            
            pkt.pts= c->coded_frame->pts;
            if(c->coded_frame->key_frame)
                pkt.flags |= PKT_FLAG_KEY;
            pkt.stream_index= st->index;
            pkt.data= video_outbuf;
            pkt.size= out_size;
            
            /* write the compressed frame in the media file */
            ret = av_write_frame(oc, &pkt);
        } else {
            ret = 0;
        }
    }
    if (ret != 0) {
        fprintf(stderr, "Error while writing video frame\n");
        exit(1);
    }
    frame_count++;
}

void close_video(AVFormatContext *oc, AVStream *st)
{
    avcodec_close(st->codec);
    av_free(picture->data[0]);
    av_free(picture);
    if (tmp_picture) {
        av_free(tmp_picture->data[0]);
        av_free(tmp_picture);
    }
    av_free(video_outbuf);
}

static const char *filename;
static AVOutputFormat *fmt;
static AVFormatContext *oc;
static AVStream *video_st;
static double video_pts;

static int movie_open(int w, int h) {
    if (fmt->video_codec != CODEC_ID_NONE) {
        video_st = add_video_stream(oc, fmt->video_codec, w, h);
    } else
	    return 1;

    /* set the output parameters (must be done even if no
       parameters). */
    if (av_set_parameters(oc, NULL) < 0) {
        fprintf(stderr, "Invalid output format parameters\n");
        return 2;
    }

    dump_format(oc, 0, filename, 1);

    /* now that all the parameters are set, we can open the audio and
       video codecs and allocate the necessary encode buffers */
    if (video_st)
        open_video(oc, video_st);

    /* open the output file, if needed */
    if (!(fmt->flags & AVFMT_NOFILE)) {
        if (url_fopen(&oc->pb, filename, URL_WRONLY) < 0) {
            fprintf(stderr, "Could not open '%s'\n", filename);
            return 3;
        }
    }
    
    /* write the stream header, if any */
    av_write_header(oc);

    return 0;
}

static int movie_close() {
    int i;

     /* close each codec */
    close_video(oc, video_st);

    /* write the trailer, if any */
    av_write_trailer(oc);
    
    /* free the streams */
    for(i = 0; i < oc->nb_streams; i++) {
        av_freep(&oc->streams[i]);
    }

    if (!(fmt->flags & AVFMT_NOFILE)) {
        /* close the output file */
        url_fclose(&oc->pb);
    }

    /* free the stream */
    av_free(oc);

}

static rfbBool quit=FALSE;
static void signal_handler(int signal) {
	fprintf(stderr,"Cleaning up.\n");
	quit=TRUE;
}

/**************************************************************/
/* VNC callback functions */
static rfbBool resize(rfbClient* client) {
	static rfbBool first=TRUE;
	if(!first) {
		movie_close();
		perror("I don't know yet how to change resolutions!\n");
	}
	movie_open(client->width, client->height);
	signal(SIGINT,signal_handler);
	if(tmp_picture)
		client->frameBuffer=tmp_picture->data[0];
	else
		client->frameBuffer=picture->data[0];
	return TRUE;
}

static void update(rfbClient* client,int x,int y,int w,int h) {
}

/**************************************************************/
/* media file output */

int main(int argc, char **argv)
{
    time_t stop=0;
    rfbClient* client;
    int i,j;

    /* get a vnc client structure (don't connect yet). */
    client = rfbGetClient(5,3,2);
    client->format.redShift=11; client->format.redMax=31;
    client->format.greenShift=5; client->format.greenMax=63;
    client->format.blueShift=0; client->format.blueMax=31;

    /* initialize libavcodec, and register all codecs and formats */
    av_register_all();
   
    if(!strncmp(argv[argc-1],":",1) ||
	!strncmp(argv[argc-1],"127.0.0.1",9) ||
	!strncmp(argv[argc-1],"localhost",9))
	    client->appData.encodingsString="raw";

    filename=0;
    for(i=1;i<argc;i++) {
	    j=i;
	    if(argc>i+1 && !strcmp("-o",argv[i])) {
		    filename=argv[2];
		    j+=2;
	    } else if(argc>i+1 && !strcmp("-t",argv[i])) {
		    stop=time(0)+atoi(argv[i+1]);
		    j+=2;
	    }
	    if(j>i) {
		    argc-=j-i;
		    memmove(argv+i,argv+j,(argc-i)*sizeof(char*));
		    i--;
	    }
    }

  
    /* auto detect the output format from the name. default is
       mpeg. */
    fmt = filename?guess_format(NULL, filename, NULL):0;
    if (!fmt) {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        fmt = guess_format("mpeg", NULL, NULL);
    }
    if (!fmt) {
        fprintf(stderr, "Could not find suitable output format\n");
        exit(1);
    }
    
    /* allocate the output media context */
    oc = av_alloc_format_context();
    if (!oc) {
        fprintf(stderr, "Memory error\n");
        exit(1);
    }
    oc->oformat = fmt;
    snprintf(oc->filename, sizeof(oc->filename), "%s", filename);

    /* add the audio and video streams using the default format codecs
       and initialize the codecs */
    video_st = NULL;

    /* open VNC connection */
    client->MallocFrameBuffer=resize;
    client->GotFrameBufferUpdate=update;
    if(!rfbInitClient(client,&argc,argv)) {
        printf("usage: %s [-o output_file] [-t seconds] server:port\n"
	       "Shoot a movie from a VNC server.\n", argv[0]);
        exit(1);
    }
    if(client->serverPort==-1)
      client->vncRec->doNotSleep = TRUE; /* vncrec playback */
    
     /* main loop */

    while(!quit) {
	int i=WaitForMessage(client,1000000/STREAM_FRAME_RATE);
	if(i<0) {
		movie_close();
		return 0;
	}
	if(i)
		if(!HandleRFBServerMessage(client))
			quit=TRUE;
	else {
	        /* compute current audio and video time */
               	video_pts = (double)video_st->pts.val * video_st->time_base.num / video_st->time_base.den;

        	/* write interleaved audio and video frames */
	        write_video_frame(oc, video_st);
	}
	if(stop!=0 && stop<time(0))
		quit=TRUE;
    }

    movie_close();
    return 0;
}
