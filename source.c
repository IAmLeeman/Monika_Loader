#include <ppu-lv2.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include <sysutil/video.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>

#include <io/pad.h>
#include "rsxutil.h"

#include <audio/audio.h>
//#include <cell/fs/cell_fs_file_api.h>
#include <sysmodule/sysmodule.h>

#define MAX_BUFFERS 2
#define IMAGE_WIDTH 1280
#define IMAGE_HEIGHT 720

gcmContextData *context;
uint8_t *rawTexture = NULL;
void *host_addr = NULL;

void* load_raw_argb(const char* path, int width, int height) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        printf("Failed to open image: %s\n", path);
        return NULL;
    }

    // For now, hardcode image dimensions — update later to parse headers if needed
   
    size_t imageSize = (width) * (height) * 4;

    void* buffer = malloc(imageSize);
    fread(buffer, 1, imageSize, file);
    fclose(file);

    printf("Image loaded successfully (%d bytes): %s\n", (int)imageSize, path);
    return buffer;
}

void drawFrame(rsxBuffer *buffer, long frame) {
  s32 i, j;
  for(i = 0; i < buffer->height; i++) {
    s32 color = (i / (buffer->height * 1.0) * 256);
    // This should make a nice black to green gradient
    color = (color << 8) | ((frame % 255) << 16);
    for(j = 0; j < buffer->width; j++)
      buffer->ptr[i* buffer->width + j] = color;
  }
}

void drawImage(rsxBuffer *buffer, const u8 *image_data, int image_width, int image_height){
  for (int y = 0; y < buffer->height && y < image_height; y++) {
    for (int x = 0; x < buffer->width && x < image_width; x++) {
      int i = y * image_width + x;
      int r = image_data[i * 4 + 0];
      int g = image_data[i * 4 + 1];
      int b = image_data[i * 4 + 2];
      int a = image_data[i * 4 + 3];

      // Pack ARGB to match RSX format
      buffer->ptr[y * buffer->width + x] = (a << 24) | (r << 16) | (g << 8) | b;
    }
  }
}

int main(s32 argc, const char* argv[])
{
  //sysModuleLoad(SYSMODULE_FS);
  //sysModuleLoad(SYSMODULE_AUDIO);
  //cellAudioInit();
  
  rsxBuffer buffers[MAX_BUFFERS];
  int currentBuffer = 0;
  padInfo padinfo;
  padData paddata;
  u16 width = 1280;
  u16 height = 720;
  int i;
  
  void* imageData = load_raw_argb("/dev_hdd0/images/m_argb.raw", width, height);
	
  /* Allocate a 1Mb buffer, aligned to a 1Mb boundary                          
   * to be our shared IO memory with the RSX. */
  host_addr = memalign (1024*1024, HOST_SIZE);
  context = initScreen (host_addr, HOST_SIZE);
  ioPadInit(7);

  getResolution(&width, &height);
  for (i = 0; i < MAX_BUFFERS; i++)
    makeBuffer( &buffers[i], width, height, i);

  flip(context, MAX_BUFFERS - 1);

  long frame = 0; // To keep track of how many frames we have rendered.
	
  // Ok, everything is setup. Now for the main loop.
  while(1){
    // Check the pads.
    ioPadGetInfo(&padinfo);
    for(i=0; i<MAX_PADS; i++){
      if(padinfo.status[i]){
	ioPadGetData(i, &paddata);
				
	if(paddata.BTN_START){
	  goto end;
	}
      }
			
    }

    waitFlip(); // Wait for the last flip to finish, so we can draw to the old buffer
    //drawFrame(&buffers[currentBuffer], frame++); // Draw into the unused buffer
    drawImage(&buffers[currentBuffer], imageData, IMAGE_WIDTH, IMAGE_HEIGHT);
    flip(context, buffers[currentBuffer].id); // Flip buffer onto screen

    currentBuffer++;
    if (currentBuffer >= MAX_BUFFERS)
      currentBuffer = 0;
  }
  
 end:

  gcmSetWaitFlip(context);
  for (i = 0; i < MAX_BUFFERS; i++)
    rsxFree(buffers[i].ptr);

  rsxFinish(context, 1);
  free(host_addr);
  ioPadEnd();
	
  return 0;
}