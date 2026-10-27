#include "stubs/fake.h"
#include "../components/camera_link_sender/camera_link_sender.h"
#include "../components/camera_link_viewer/camera_link_viewer.h"
#include <iostream>
using namespace esphome;
using namespace camera_link_protocol;
std::vector<uint8_t> jpeg{0xFF,0xD8,0xFF,0xC0,0,11,8,0,240,1,64,1,1,0x11,0,0xFF,0xD9};
std::vector<uint8_t> wire(Type type,uint32_t seq,const std::vector<uint8_t> &data={}) {
 std::vector<uint8_t> result(HEADER_SIZE+data.size());
 make_header(result.data(),type,seq,data.data(),data.size());
 std::copy(data.begin(),data.end(),result.begin()+HEADER_SIZE);return result;
}
void input(uart::UARTComponent &uart,const std::vector<uint8_t>&data){uart.rx.insert(uart.rx.end(),data.begin(),data.end());}
std::shared_ptr<esp32_camera::ESP32CameraImage> make_image(uint32_t when) {
 auto result=std::make_shared<esp32_camera::ESP32CameraImage>();result->data=jpeg;
 result->fb.timestamp.tv_sec=when/1000;result->fb.timestamp.tv_usec=(when%1000)*1000;return result;
}
int main(){
 uart::UARTComponent tx;
 esp32_camera::ESP32Camera camera;
 camera_link_sender::CameraLinkSender sender;
 sender.set_uart_parent(&tx);sender.set_camera(&camera);sender.setup();
 fake_now=1000;input(tx,wire(REQUEST,1));sender.loop();assert(camera.requests==1);
 fake_now=3100;sender.on_camera_image(make_image(3100));sender.loop();assert(tx.tx.empty());
 fake_now=4000;input(tx,wire(REQUEST,2));sender.loop();
 sender.on_camera_image(make_image(3900));assert(tx.tx.empty());
 sender.loop();assert(camera.requests==3); // Stale capture retries on next loop.
 fake_now=4100;sender.on_camera_image(make_image(4100));sender.loop();sender.loop();
 assert(sender.frames_sent()==1&&fake_high_frequency==0);
 fake_now=5000;input(tx,wire(REQUEST,3));sender.loop();sender.on_camera_image(make_image(5000));
 tx.writable=0;fake_now=7101;sender.loop();assert(fake_high_frequency==0);
 sender.on_shutdown();
 uart::UARTComponent rx;text_sensor::TextSensor status;lv_obj_t parent;
 camera_link_viewer::CameraLinkViewer viewer;
 viewer.set_uart_parent(&rx);viewer.set_parent_widget(&parent);viewer.set_status(&status);viewer.setup();
 fake_now=10000;viewer.set_enabled(true);viewer.loop();
 uint32_t seq=read32(rx.tx.data()+8);auto reply=wire(FRAME,seq,jpeg);
 fake_now=11900;input(rx,{reply.begin(),reply.end()-1});viewer.loop();
 fake_now=12100;input(rx,{reply.back()});viewer.loop();
 assert(viewer.frames_received()==0&&status.state=="Camera unavailable"&&fake_end_count==0);
 // Decode that crosses the absolute deadline never publishes.
 fake_now=12300;viewer.loop();seq=read32(rx.tx.data()+rx.tx.size()-HEADER_SIZE+8);
 fake_now=14200;fake_decode_delay=200;input(rx,wire(FRAME,seq,jpeg));viewer.loop();
 assert(viewer.frames_received()==0&&fake_end_count==0&&fake_release_count==1);
 fake_decode_delay=0;fake_now=14600;viewer.loop();seq=read32(rx.tx.data()+rx.tx.size()-HEADER_SIZE+8);
 fake_now=14700;fake_finished=false;input(rx,wire(FRAME,seq,jpeg));viewer.loop();
 assert(viewer.frames_received()==0&&fake_end_count==0&&status.state=="Camera decode failed");
 fake_finished=true;fake_now=14900;viewer.loop();seq=read32(rx.tx.data()+rx.tx.size()-HEADER_SIZE+8);
 fake_now=15000;input(rx,wire(FRAME,seq,jpeg));viewer.loop();
 assert(viewer.frames_received()==1&&fake_end_count==1&&!fake_image->hidden&&status.state=="Live");
 fake_now=17100;viewer.loop();assert(fake_image->hidden&&status.state=="Camera unavailable");
 viewer.set_enabled(false);assert(fake_high_frequency==0&&status.state=="Idle");
 // OTA begin pauses; failed OTA restores only for the page that is still active.
 viewer.set_enabled(true);viewer.set_enabled(false);assert(fake_high_frequency==0);
 viewer.set_enabled(true);assert(fake_high_frequency==1);
 viewer.set_enabled(false);viewer.set_enabled(false);assert(fake_high_frequency==0);
 viewer.set_enabled(true);viewer.on_shutdown();assert(fake_high_frequency==0);
 for(auto *allocation:fake_allocations) std::free(allocation);
 fake_allocations.clear();
 std::cout<<"Actual sender/viewer: late callbacks/replies, decode deadline/failure, freshness retry, stale blanking, and shutdown passed\n";
}
