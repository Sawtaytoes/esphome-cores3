#pragma once
#ifdef CAMERA_LINK_REAL_JPEGDEC
#include_next <JPEGDEC.h>
#else
#include "fake.h"
constexpr int JPEG_MODE_BASELINE=0,RGB565_LITTLE_ENDIAN=0;
struct JPEGDRAW {int x,y,iWidth,iHeight,iWidthUsed,iBpp;uint16_t *pPixels;void *pUser;};
class JPEGDEC {
 int (*draw_)(JPEGDRAW *){};void *user_{};
 public:
 bool openRAM(uint8_t *,size_t,int (*draw)(JPEGDRAW *)){draw_=draw;return fake_begin_ok;}
 int getJPEGType(){return JPEG_MODE_BASELINE;}int getWidth(){return 320;}int getHeight(){return 240;}
 void setUserPointer(void *p){user_=p;}void setPixelType(int){}
 bool decode(int,int,int){
  assert(!fake_image||fake_image->hidden);assert(!fake_image||!fake_image->source);
  fake_now+=fake_decode_delay;if(!fake_finished||!fake_feed_ok)return false;
  static uint16_t pixels[320*240]{};JPEGDRAW block{0,0,320,240,320,16,pixels,user_};
  if(!draw_(&block)) return false;
  fake_end_count++;
  return true;
 }
 void close(){fake_release_count++;}
};
#endif
