#include "z80_driver.h"
namespace mda {
Z80Driver::Z80Driver(SoundBus& bus):bus_(bus){ reset(); }
uint8_t Z80Driver::read(void* ctx,uint16_t a){return static_cast<Z80Driver*>(ctx)->bus_.z80Read(a);}
void Z80Driver::write(void* ctx,uint16_t a,uint8_t v){
    auto* self=static_cast<Z80Driver*>(ctx);
    self->bus_.setCycle(self->core_.cyc);
    self->bus_.z80Write(a,v);
}
uint8_t Z80Driver::in(z80*,uint8_t){return 0xff;}
void Z80Driver::out(z80*,uint8_t,uint8_t){}
void Z80Driver::reset(){
    z80_init(&core_);
    core_.userdata=this; core_.read_byte=&read; core_.write_byte=&write;
    core_.port_in=&in; core_.port_out=&out;
}
void Z80Driver::load(const uint8_t* data,size_t count){bus_.reset();bus_.loadZ80(data,count);reset();}
uint64_t Z80Driver::runCycles(uint64_t amount){
    const auto start=uint64_t(core_.cyc);
    while(uint64_t(core_.cyc)-start < amount) z80_step(&core_);
    bus_.setCycle(core_.cyc);
    return uint64_t(core_.cyc)-start;
}
}
