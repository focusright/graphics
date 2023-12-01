//tcc\tcc source\graphics.c source\dos.c
//wasm\node wasm\wajicup.js source\graphics.c source\dos.c graphics.html

#include <stdlib.h>
#include "dos.h"

int main(int argc, char* argv[]) {
    setvideomode(videomode_320x200);

    while(!shuttingdown()) {
        waitvbl();
        for(int i=0; i<50; ++i) {
            setcolor(rand() % 256);
            line(rand() % 320, rand() % 200, rand() % 320, rand() % 200);
            setcolor(rand() % 256);
            fillcircle(rand() % 320, rand() % 200, rand() % 30);
            setcolor(rand() % 256);
            circle(rand() % 320, rand() % 200, rand() % 30);
        }
        if( keystate( KEY_ESCAPE ) )  { break; }
    }
    return 0;
}