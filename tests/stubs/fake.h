#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include <sys/time.h>
inline uint32_t fake_now{}, fake_decode_delay{};
inline bool fake_finished=true, fake_feed_ok=true, fake_begin_ok=true;
inline std::vector<void *> fake_allocations;
inline int fake_end_count{}, fake_release_count{}, fake_high_frequency{};
struct lv_obj_t { bool hidden=true; const void *source=nullptr; };
inline lv_obj_t *fake_image{};
constexpr int LV_OBJ_FLAG_HIDDEN=1;
inline void lv_obj_add_flag(lv_obj_t *o,int) {o->hidden=true;}
inline void lv_obj_remove_flag(lv_obj_t *o,int) {o->hidden=false;}
inline void lv_image_set_src(lv_obj_t *o,const void *src) {o->source=src;}
inline void lv_image_cache_drop(const void *) {}
inline lv_obj_t *lv_image_create(lv_obj_t *) {static lv_obj_t obj; fake_image=&obj;return &obj;}
inline void lv_obj_center(lv_obj_t *) {}
inline void lv_obj_invalidate(lv_obj_t *) {}
constexpr int MALLOC_CAP_INTERNAL=1,MALLOC_CAP_8BIT=2,MALLOC_CAP_SPIRAM=4;
inline size_t heap_caps_get_free_size(int) {return 100000;}
inline size_t heap_caps_get_largest_free_block(int) {return 80000;}
inline int64_t esp_timer_get_time() {return int64_t(fake_now)*1000;}
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGCONFIG(...) ((void)0)
namespace esphome {
inline uint32_t millis() {return fake_now;}
inline uint32_t random_uint32() {return 0x12345678;}
class Component {public: virtual ~Component()=default;virtual void setup(){};virtual void loop(){};virtual void dump_config(){};virtual void on_shutdown(){};bool is_failed()const{return failed_;}void mark_failed(){failed_=true;}private:bool failed_{};};
class HighFrequencyLoopRequester {bool active_{};public:void start(){if(!active_){active_=true;fake_high_frequency++;}}void stop(){if(active_){active_=false;fake_high_frequency--;}}};
template<class T> class RAMAllocator {public:enum{ALLOC_EXTERNAL=1};explicit RAMAllocator(int){}T *allocate(size_t n){auto *p=static_cast<T *>(std::calloc(n,sizeof(T)));fake_allocations.push_back(p);return p;}void deallocate(T *p,size_t){fake_allocations.erase(std::remove(fake_allocations.begin(),fake_allocations.end(),p),fake_allocations.end());std::free(p);}};
namespace uart {
class UARTComponent {public:std::deque<uint8_t> rx;std::vector<uint8_t> tx;int writable=8192;int available_for_write(){return writable;}};
class UARTDevice {public:UARTComponent *parent_{};void set_uart_parent(UARTComponent *p){parent_=p;}int available(){return parent_->rx.size();}bool read_byte(uint8_t *p){if(parent_->rx.empty())return false;*p=parent_->rx.front();parent_->rx.pop_front();return true;}void write_array(const uint8_t *p,size_t n){parent_->tx.insert(parent_->tx.end(),p,p+n);}};
}
namespace camera {
enum CameraRequester {WEB_REQUESTER=1};
class CameraImage {public:virtual ~CameraImage()=default;virtual size_t get_data_length()=0;virtual uint8_t *get_data_buffer()=0;virtual bool was_requested_by(CameraRequester)const=0;};
class CameraListener {public:virtual ~CameraListener()=default;virtual void on_camera_image(const std::shared_ptr<CameraImage>&)=0;};
}
namespace esp32_camera {
struct camera_fb_t {timeval timestamp;};
class ESP32CameraImage:public camera::CameraImage {public:std::vector<uint8_t> data;camera_fb_t fb{};camera_fb_t *get_raw_buffer(){return &fb;}size_t get_data_length()override{return data.size();}uint8_t *get_data_buffer()override{return data.data();}bool was_requested_by(camera::CameraRequester)const override{return true;}};
class ESP32Camera:public Component {public:int requests{};void add_listener(camera::CameraListener *){}void request_image(camera::CameraRequester){requests++;}};
}
namespace text_sensor {class TextSensor {public:std::string state;bool has_state()const{return !state.empty();}void publish_state(const char *s){state=s;}};}
namespace image {enum{IMAGE_TYPE_RGB565,TRANSPARENCY_OPAQUE};}
namespace runtime_image {
enum{JPEG};
class RuntimeImage {public:RuntimeImage(int,int,int,void *,bool,int,int){}bool set_external_buffer(uint8_t *,int,int){return true;}bool begin_decode(size_t){return fake_begin_ok;}int feed_data(uint8_t *,size_t n){assert(!fake_image||fake_image->hidden);assert(!fake_image||!fake_image->source);fake_now+=fake_decode_delay;return fake_feed_ok?int(n):-1;}bool is_decode_finished(){return fake_finished;}bool end_decode(){fake_end_count++;return true;}void release(){fake_release_count++;}const void *get_lv_image_dsc(){return this;}};
}
}
