#include "stubs/fake.h"
#include "../components/camera_link_viewer/camera_link_viewer.h"
#include <fstream>
#include <iostream>
using namespace esphome;
using namespace camera_link_protocol;
std::vector<uint8_t> file(const char *path) {
 std::ifstream input(path,std::ios::binary);assert(input.good());
 return std::vector<uint8_t>((std::istreambuf_iterator<char>(input)),{});
}
int main(){
 uart::UARTComponent uart;text_sensor::TextSensor status;lv_obj_t parent;
 camera_link_viewer::CameraLinkViewer viewer;
 viewer.set_uart_parent(&uart);viewer.set_parent_widget(&parent);viewer.set_status(&status);viewer.setup();
 fake_now=1000;viewer.set_enabled(true);
 for(auto name:{"valid","truncated","invalid-entropy","progressive","wrong-size","valid"}) {
  fake_now+=200;viewer.loop();
  auto jpeg=file((std::string("tests/fixtures/")+name+".jpg").c_str());
  std::vector<uint8_t> wire(HEADER_SIZE+jpeg.size());
  make_header(wire.data(),FRAME,read32(uart.tx.data()+uart.tx.size()-HEADER_SIZE+8),jpeg.data(),jpeg.size());
  std::copy(jpeg.begin(),jpeg.end(),wire.begin()+HEADER_SIZE);
  uart.rx.insert(uart.rx.end(),wire.begin(),wire.end());fake_now+=20;
  do {viewer.loop();}while(!uart.rx.empty());
  bool valid=std::string(name)=="valid";
  assert((status.state=="Live")==valid);
  assert(fake_image->hidden!=valid);
  if(valid) {
   auto data=viewer.get_data_start();assert(data);
   auto pixel=[&](size_t x){return unsigned(data[x*2])|(unsigned(data[x*2+1])<<8);};
   assert(((pixel(20)>>11)&31)>25 && (pixel(20)&31)<5); // red, RGB565 little endian
   assert((pixel(200)&31)>25 && ((pixel(200)>>11)&31)<5); // blue
  }
  std::cout<<name<<": "<<status.state<<"\n";
 }
 assert(viewer.frames_received()==2);
 viewer.on_shutdown();
 for(auto *p:fake_allocations)std::free(p);
 fake_allocations.clear();
}
