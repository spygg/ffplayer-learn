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
#include "SDL.h"

//#define SCALE_OUTPUT
#define REFRESH_EVENT  (SDL_USEREVENT + 1)

int updateThread(void *pArg){

    while(1){
        if(*((int*)pArg) == 1)
            break;

        SDL_Event event;
        event.type = REFRESH_EVENT;
        SDL_PushEvent(&event);
        SDL_Delay(40);
    }

    return 0;
}

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
    int iVideoWidth = 0;
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


    SDL_Window *pWindow = NULL;
    SDL_Renderer *pRenderer = NULL;
    SDL_Texture  *pTexture = NULL;
    SDL_Thread *pUpdateThread = NULL;
    int iWindowWidth = 0;
    int iWindowHeight = 0;
    int iBufferSize = 0;
    int iQuitFlag = 0;
    SDL_Rect rect;
    SDL_Event event;


    char *szFileName = NULL;

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

    pFrameYUV = av_frame_alloc();
    pFrame = av_frame_alloc();
    pPacket = av_packet_alloc();

    //计算一个标准画面的大小
    iPicSize420P = av_image_get_buffer_size(AV_PIX_FMT_YUV420P,  pCodecContext->width, pCodecContext->height, 1);
    out_buffer = (unsigned char *)av_malloc(iPicSize420P);
    
    av_image_fill_arrays(pFrameYUV->data, 
        pFrameYUV->linesize, 
        out_buffer,
        AV_PIX_FMT_YUV420P,
        pCodecContext->width, 
        pCodecContext->height,
        1
    );


    iWindowWidth = pCodecContext->width;
    iWindowHeight = pCodecContext->height;
    iBufferSize = iWindowWidth * iWindowHeight * 1.5;

    if(SDL_Init(SDL_INIT_VIDEO)) {
        printf("%s\n", "初始化SDL失败!");
        return -1;
    }

    pWindow = SDL_CreateWindow(argv[1],
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        iWindowWidth,
        iWindowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    if(pWindow == NULL){
        printf("%s\n", "SDL_CreateWindow 失败!");
        return -1;
    }

    pRenderer = SDL_CreateRenderer(pWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if(pRenderer == NULL){
        printf("%s\n", "SDL_CreateRenderer 失败!");
        return -1;
    }

    pTexture = SDL_CreateTexture(pRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, iWindowWidth, iWindowHeight);
    if(pTexture == NULL){
        printf("%s\n", "SDL_CreateTexture 失败!");
        return -1;
    }

    pUpdateThread = SDL_CreateThread(updateThread, "decoder", &iQuitFlag);
    if (!pUpdateThread) {
        printf("%s\n", "SDL_CreateTexture 失败!");
        return -1;
    }


    rect.x = 0;
    rect.y = 0;
    rect.w = iWindowWidth;
    rect.h = iWindowHeight;

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
                SDL_WaitEvent(&event);

                if(event.type == REFRESH_EVENT){
                         sws_scale(img_convert_ctx, 
                        (const uint8_t* const*)pFrame->data, 
                        pFrame->linesize, 
                        0, 
                        pCodecContext->height, 
                        pFrameYUV->data, 
                        pFrameYUV->linesize
                    );  

                    SDL_UpdateTexture(pTexture, NULL, pFrameYUV->data[0], iWindowWidth);
                    SDL_RenderClear( pRenderer );   
                    SDL_RenderCopy(pRenderer, pTexture, NULL, &rect);
                    SDL_RenderPresent(pRenderer);
                }
               
               if(event.type == SDL_QUIT){
                    printf("%s\n", "收到退出事件!");
                    iQuitFlag = 1;
                    break;
                }

                if(event.type == SDL_WINDOWEVENT){
                   SDL_GetWindowSize(pWindow, &rect.w, &rect.h);
                }
                // //RGB 
                // if(fpYUV){
                //     int y_size = pCodecContext->width * pCodecContext->height;  
                //     fwrite(pFrameYUV->data[0], 1, y_size, fpYUV);    //Y 
                //     fwrite(pFrameYUV->data[1], 1, y_size/4, fpYUV);  //U
                //     fwrite(pFrameYUV->data[2], 1, y_size/4, fpYUV);  //V

                // }
            }

            if(iQuitFlag){
                break;
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
                // if(fpYUV){
                //     int y_size = pCodecContext->width * pCodecContext->height;  
                //     fwrite(pFrameYUV->data[0], 1, y_size, fpYUV);    //Y 
                //     fwrite(pFrameYUV->data[1], 1, y_size/4, fpYUV);  //U
                //     fwrite(pFrameYUV->data[2], 1, y_size/4, fpYUV);  //V

                // }
            }
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