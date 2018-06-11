//编译
//gcc -o player player.c `pkg-config --cflags --libs sdl2` `pkg-config --cflags --libs ffmpeg`
//gcc -o player player.c -D_REENTRANT -I/usr/include/SDL2 -lSDL2 -lavdevice -lavfilter -lswscale -lavformat -lswresample -lavutil -lavcodec -pthread -lm -lz -lva

#include <stdio.h>

#include "libavutil/avstring.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/time.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"

typedef int (*decodeCallBack)(void *pArg);

int callBack(void *pArg){

    return 0;
}

int decode(AVCodecContext *pCodecContext, AVFrame *pFrame, AVPacket *pPacket, decodeCallBack function){
    
    // int err = 0;
    // err = avcodec_send_packet(pCodecContext, pPacket);
    // if(err != 0){
    //     if(AVERROR(EAGAIN) != err)
    //         return -1;
    //     printf("%s, %d\n", "发送视频帧失败!", err);                
    // }

    // //解码
    // while(avcodec_receive_frame(pCodecContext, pFrame) == 0){

    //     sws_scale(img_convert_ctx, 
    //         (const uint8_t* const*)pFrame->data, 
    //         pFrame->linesize, 
    //         0, 
    //         pCodecContext->height, 
    //         pFrameYUV->data, 
    //         pFrameYUV->linesize
    //     );  
              
              
    //     //RGB  
       
    //     if(fpYUV){
    //         int y_size = pCodecContext->width * pCodecContext->height;  
    //         fwrite(pFrameYUV->data[0], 1, y_size, fpYUV);    //Y 
    //         fwrite(pFrameYUV->data[1], 1, y_size/4, fpYUV);  //U
    //         fwrite(pFrameYUV->data[2], 1, y_size/4, fpYUV);  //V

    //     }
    // }


    function(NULL);
}

int main(int argc, char *argv[]){

    int err = 0;
    int iVideoChannel = -1;
    int iPicSize420P = 0;
    AVFormatContext *pFormatContext = NULL;
    AVCodecContext *pCodecContext = NULL;
    AVPacket *pPacket = NULL;
    AVFrame *pFrame = NULL;
    AVFrame *pFrameYUV = NULL;
    AVCodecParameters *pVideoChannelCodecPara = NULL;
    AVCodec *pCodec = NULL;

    unsigned char *out_buffer = NULL;
    struct SwsContext *img_convert_ctx;

    char *szFileName = NULL;
    FILE *fpYUV = NULL;
    char szYUVFileName[255] = {0};

    if(argc < 2){
        printf("No input file!\n");
        return -1;
    }

    szFileName = argv[1];

    pFormatContext = avformat_alloc_context();
    err = avformat_open_input(&pFormatContext, szFileName, NULL, NULL);
    if(err < 0){
        printf("打开视频流失败!\n");
        return -1;
    }

    err = avformat_find_stream_info(pFormatContext, NULL);
    if(err < 0){
        printf("avformat_find_stream_info failed! %d\n", __LINE__);
        return -1;
    }

    printf("封装格式的长名称: %s\n", pFormatContext->iformat->long_name);
    printf("封装扩展名: %s\n", pFormatContext->iformat->extensions);

    for(int i = 0; i < pFormatContext->nb_streams; i++){
        pVideoChannelCodecPara = pFormatContext->streams[i]->codecpar;
        if(pVideoChannelCodecPara->codec_type == AVMEDIA_TYPE_VIDEO){
            iVideoChannel = i;

            printf("共有: %d个文件流, 视频在第 %d 通道\n", pFormatContext->nb_streams, i);
            printf("视频尺寸: %d*%d\n", pVideoChannelCodecPara->width, pVideoChannelCodecPara->height);
            break;
        }
    }

    if(iVideoChannel < 0){
        printf("%s\n", "没找到视频流");
        return -1;
    }

    pCodecContext =  avcodec_alloc_context3(NULL);
    if (!pCodecContext){
        printf("%s\n", "avcodec_alloc_context3");
        return -1;
    }

    err = avcodec_parameters_to_context(pCodecContext, pVideoChannelCodecPara);
    if (err < 0){
        printf("%s\n", "avcodec_parameters_to_context");
        return -1;
    }


    pCodec = avcodec_find_decoder(pVideoChannelCodecPara->codec_id);
    if(pCodec == NULL){
        printf("%s\n", "avcodec_find_decoder");
        return -1;
    }

    printf("编解码器名: %s\n", pCodec->long_name);

    err = avcodec_open2(pCodecContext, pCodec, NULL);
    if(err){
        printf("%s\n", "avcodec_open2");
        return -1;
    }

    //前戏完毕
    printf("--------------- File Information ----------------\n");
    av_dump_format(pFormatContext, 0, szFileName, 0);
    printf("-------------------------------------------------\n");

    img_convert_ctx = sws_getContext(pCodecContext->width, pCodecContext->height, pCodecContext->pix_fmt, 
        pCodecContext->width, pCodecContext->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL); 

    pFrame = av_frame_alloc();
    pFrameYUV = av_frame_alloc();
    pPacket = av_packet_alloc();
    

    //计算一个标准画面的大小
    iPicSize420P = av_image_get_buffer_size(AV_PIX_FMT_YUV420P,  pCodecContext->width, pCodecContext->height, 1);
    out_buffer = (unsigned char *)av_malloc(iPicSize420P);
    

    // int av_image_fill_arrays(uint8_t *dst_data[4], int dst_linesize[4],
    //                      const uint8_t *src,
    //                      enum AVPixelFormat pix_fmt, int width, int height, int align);
    av_image_fill_arrays(pFrameYUV->data, 
        pFrameYUV->linesize, 
        out_buffer,
        AV_PIX_FMT_YUV420P,
        pCodecContext->width, 
        pCodecContext->height,
        1
    );
    // char *pDot = strstr(szFileName, ".");
    // if(pDot){
    //     strncpy(szYUVFileName, szFileName, pDot - szFileName);
    // }
    // printf("%s\n", szYUVFileName);

    sprintf(szYUVFileName, "%s_%d_%d.yuv", szFileName, pCodecContext->width, pCodecContext->height);
    fpYUV = fopen(szYUVFileName, "wb");

    while(av_read_frame(pFormatContext, pPacket) >= 0){
        // printf("码流: %d, 时间戳: %ld, 大小:%d\n", pPacket->stream_index, pPacket->pts, pPacket->size);

        //视频帧
        if(pPacket->stream_index == iVideoChannel){

            decode(pCodecContext, pFrame, pPacket, callBack);

            err = avcodec_send_packet(pCodecContext, pPacket);
            if(err != 0){
                if(AVERROR(EAGAIN) == err)
                    continue;
                printf("%s, %d\n", "发送视频帧失败!", err);                
            }

            //解码
            while(avcodec_receive_frame(pCodecContext, pFrame) == 0){
                // printf("%d, %d\n", pFrame->width, pFrame->height);

                // printf("一行宽度: %d, 显示的: %d\n", pFrame->linesize[0], pFrame->display_picture_number);

        

                sws_scale(img_convert_ctx, 
                    (const uint8_t* const*)pFrame->data, 
                    pFrame->linesize, 
                    0, 
                    pCodecContext->height, 
                    pFrameYUV->data, 
                    pFrameYUV->linesize
                );  
                      
                      
                //RGB  
               
                if(fpYUV){
                    int y_size = pCodecContext->width * pCodecContext->height;  
                    fwrite(pFrameYUV->data[0], 1, y_size, fpYUV);    //Y 
                    fwrite(pFrameYUV->data[1], 1, y_size/4, fpYUV);  //U
                    fwrite(pFrameYUV->data[2], 1, y_size/4, fpYUV);  //V

                }
            }
        }
    }

    // printf("码流: %d, 时间戳: %ld, 大小:%d\n", pPacket->stream_index, pPacket->pts, pPacket->size);
    //清空缓冲区
    {
        err = avcodec_send_packet(pCodecContext, NULL);
        if(err){
            printf("%s, %d\n", "发送视频帧失败!", err);                
        }

        //解码
        while(avcodec_receive_frame(pCodecContext, pFrame) == 0){
            // printf("%d, %d\n", pFrame->width, pFrame->height);
            printf("缓冲区已经清空!\n");
            if(fpYUV){
                sws_scale(img_convert_ctx, 
                    (const uint8_t* const*)pFrame->data, 
                    pFrame->linesize, 
                    0, 
                    pCodecContext->height, 
                    pFrameYUV->data, 
                    pFrameYUV->linesize
                );  
                      
                      
                //RGB  
                int y_size = pCodecContext->width * pCodecContext->height;  
                fwrite(pFrameYUV->data[0], 1, y_size, fpYUV);    //Y 
                fwrite(pFrameYUV->data[1], 1, y_size/4, fpYUV);  //U
                fwrite(pFrameYUV->data[2], 1, y_size/4, fpYUV);  //V
            }
        }
    }


    if(fpYUV){
        fclose(fpYUV);
    }

    //清理资源
    avcodec_close(pCodecContext);
    avformat_close_input(&pFormatContext);

    avcodec_free_context(&pCodecContext);
    av_frame_free(&pFrame);
    av_frame_free(&pFrameYUV);
    av_packet_free(&pPacket);


    av_free(out_buffer);
    printf("完毕退出!\n");
    return 0;
}