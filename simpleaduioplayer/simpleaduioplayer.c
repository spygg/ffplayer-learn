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

//#define SCALE_OUTPUT

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
    int iAudioChannel = -1;
    int iVideoWidth = 0;
    int iPicSize420P = 0;
    AVFormatContext *pFormatContext = NULL;
    AVCodecContext *pCodecContext = NULL;
    AVPacket *pPacket = NULL;
    AVFrame *pFrame = NULL;
    AVCodecParameters *pAudioChannelCodecPara = NULL;
    AVCodec *pCodec = NULL;

    FILE *fp = NULL;

    char *szFileName = NULL;


    if(argc < 2){
        printf("No input file!\n");
        return -1;
    }

    szFileName = argv[1];

    pFormatContext = avformat_alloc_context();
    err = avformat_open_input(&pFormatContext, szFileName, NULL, NULL);
    if(err < 0){
        printf("打开文件失败!\n");
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
        pAudioChannelCodecPara = pFormatContext->streams[i]->codecpar;
        if(pAudioChannelCodecPara->codec_type == AVMEDIA_TYPE_AUDIO){
            iAudioChannel = i;

            printf("共有: %d个文件流, 音频在第 %d 通道\n", pFormatContext->nb_streams, i);
            break;
        }
    }


    if(iAudioChannel < 0){
        printf("%s\n", "没找到音频流");
        return -1;
    }

    printf("音频采样率:%d, 声道数: %d\n", pAudioChannelCodecPara->sample_rate, pAudioChannelCodecPara->channels);

    pCodecContext =  avcodec_alloc_context3(NULL);
    if (!pCodecContext){
        printf("%s\n", "avcodec_alloc_context3");
        return -1;
    }

    err = avcodec_parameters_to_context(pCodecContext, pAudioChannelCodecPara);
    if (err < 0){
        printf("%s\n", "avcodec_parameters_to_context");
        return -1;
    }

    enum AVSampleFormat sfmt = pCodecContext->sample_fmt;

    if (av_sample_fmt_is_planar(sfmt)) {
        printf("%s\n", "就有点像视频部分的YUV数据，同样对双声道音频PCM数据，以S16P为例，存储就可能是"
        "plane 0: LLLLLLLLLLLLLLLLLLLLLLLLLL..."
        "plane 1: RRRRRRRRRRRRRRRRRRRR....");
    }

    pCodec = avcodec_find_decoder(pAudioChannelCodecPara->codec_id);
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


    pFrame = av_frame_alloc();
    pPacket = av_packet_alloc();
    

    fp = fopen("output.audio", "wb");

    while(av_read_frame(pFormatContext, pPacket) >= 0){
        // printf("码流: %d, 时间戳: %ld, 大小:%d\n", pPacket->stream_index, pPacket->pts, pPacket->size);

        //视频帧
        if(pPacket->stream_index == iAudioChannel){

            decode(pCodecContext, pFrame, pPacket, callBack);

            err = avcodec_send_packet(pCodecContext, pPacket);
            if(err != 0){
                if(AVERROR(EAGAIN) == err)
                    continue;
                printf("%s, %d\n", "发送视频帧失败!", err);                
            }

            //解码
            while(avcodec_receive_frame(pCodecContext, pFrame) == 0){
                // printf("音频帧大小:%d, nb_samples: %d, channel_layout: %ld\n", pFrame->linesize[0], pFrame->nb_samples, pFrame->channel_layout);
                
                if(fp){


                    int bytesPerSample = (size_t) av_get_bytes_per_sample(pCodecContext->sample_fmt);
                    // printf("%d\n", bytesPerSample);
                        //PCM采样数据的排列方式，一般是交错排列输出，AVFrame中存储PCM数据各个通道是分开存储的
                        //所以多通道的时候，需要根据PCM的格式和通道数，排列好后存储
                        if(pAudioChannelCodecPara->channels > 1){
                            //多通道的
                         
                                for (int j = 0; j< pAudioChannelCodecPara->channels; j++){
                                    fwrite(pFrame->data[j] + bytesPerSample, 1, bytesPerSample, fp);
                                }
                            
                            // av_log(NULL,AV_LOG_DEBUG,"avcodec_receive_frame ok,%d,%d",bytesPerSample*frameSize*2,avFrame->nb_samples);
                        }else{
                            //单通道的，
                            fwrite(pFrame->data[0], 1, 1 * bytesPerSample, fp);
                        }
                    

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


        }
    }



    if(fp){
        fclose(fp);
    }

    //清理资源
    avcodec_close(pCodecContext);
    avformat_close_input(&pFormatContext);

    avcodec_free_context(&pCodecContext);
    av_frame_free(&pFrame);
    av_packet_free(&pPacket);

    printf("完毕退出!\n");
    return 0;
}