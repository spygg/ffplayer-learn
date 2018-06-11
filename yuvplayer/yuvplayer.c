//编译
//gcc -o yuvplayer yuvplayer.c `pkg-config --cflags --libs sdl2` `pkg-config --cflags --libs ffmpeg`
//gcc -o yuvplayer yuvplayer.c -D_REENTRANT -I/usr/include/SDL2 -lSDL2 -lavdevice -lavfilter -lswscale -lavformat -lswresample -lavutil -lavcodec -pthread -lm -lz -lva

#include <stdio.h>
#include <SDL.h>

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

int main(int argc, char *argv[]){

    int err = 0;
    FILE *fpYUV = NULL;
    SDL_Window *pWindow = NULL;
    SDL_Renderer *pRenderer = NULL;
    SDL_Texture  *pTexture = NULL;
    SDL_Thread *pUpdateThread = NULL;

    char *pBuffer = NULL;
    int iWindowWidth = 0;
    int iWindowHeight = 0;
    int iBufferSize = 0;
    int iQuitFlag = 0;

    SDL_Rect rect;
    SDL_Event event;

    if(argc < 4){
        printf("Input error! eg(yuvplayer test.yuv 320 180)\n");
        return -1;
    }
    
    iWindowWidth = atoi(argv[2]);
    iWindowHeight = atoi(argv[3]);
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

    fpYUV = fopen(argv[1], "rb");
    pBuffer = (char*)malloc(iBufferSize);



    pUpdateThread = SDL_CreateThread(updateThread, "decoder", &iQuitFlag);
    if (!pUpdateThread) {
        printf("%s\n", "SDL_CreateTexture 失败!");
        return -1;
    }


    rect.x = 0;
    rect.y = 0;
    rect.w = iWindowWidth;
    rect.h = iWindowHeight;

    if(fpYUV){

        while(1){
            SDL_WaitEvent(&event);

            switch(event.type){
                case REFRESH_EVENT:
                    {
                         err = fread(pBuffer, 1, iBufferSize, fpYUV);
                        if(err == 0){
                            break;
                        }

                        SDL_UpdateTexture(pTexture, NULL, pBuffer, iWindowWidth);


                        SDL_RenderClear( pRenderer );   
                        SDL_RenderCopy(pRenderer, pTexture, NULL, &rect);
                        SDL_RenderPresent(pRenderer);

                        if(feof(fpYUV) || ferror(fpYUV)){
                            break;
                        }

                        SDL_Delay(20);
                    }
                    break;
                case SDL_QUIT:
                    {
                        printf("%s\n", "收到退出事件!");
                        iQuitFlag = 1;
                    }
                    break;
                case SDL_WINDOWEVENT:
                    {
                        //If Resize
                        SDL_GetWindowSize(pWindow, &rect.w, &rect.h);
                    }
                    break;
                default:
                    break;
           }

            if(iQuitFlag){
                break;
            }
        }

        fclose(fpYUV);
    }


    printf("完毕退出!\n");
    free(pBuffer);
    return 0;
}
