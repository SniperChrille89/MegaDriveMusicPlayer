#include "m68k_driver.h"
#include "m68k.h"
#include <stdexcept>

// Musashi's C global bus callbacks: one 68k instance is supported at a time.
static mda::M68KDriver* active68k=nullptr;
extern "C" {
unsigned int m68k_read_memory_8(unsigned int a) {return active68k?active68k->read8(a):0xff;}
unsigned int m68k_read_memory_16(unsigned int a) {return active68k?active68k->read16(a):0xffff;}
unsigned int m68k_read_memory_32(unsigned int a) {return (m68k_read_memory_16(a)<<16)|m68k_read_memory_16(a+2);}
void m68k_write_memory_8(unsigned int a,unsigned int v) {if(active68k)active68k->write8(a,uint8_t(v));}
void m68k_write_memory_16(unsigned int a,unsigned int v) {if(active68k)active68k->write16(a,uint16_t(v));}
void m68k_write_memory_32(unsigned int a,unsigned int v) {m68k_write_memory_16(a,v>>16);m68k_write_memory_16(a+2,v);}
}
namespace mda {
M68KDriver::M68KDriver(SoundBus& bus):bus_(bus) {
    if(active68k && active68k!=this) throw std::runtime_error("Only one Musashi 68000 instance supported");
    active68k=this;
    m68k_init();m68k_set_cpu_type(M68K_CPU_TYPE_68000);
}
void M68KDriver::load(const uint8_t* data,size_t count) {
    rom_.assign(data,data+count);reset();
}
void M68KDriver::reset() {m68k_pulse_reset();}
int M68KDriver::runCycles(int count) {return m68k_execute(count);}
uint32_t M68KDriver::pc() const {return m68k_get_reg(nullptr,M68K_REG_PC);}
uint8_t M68KDriver::read8(uint32_t a) const {
    a&=0xffffff;
    if(a<rom_.size())return rom_[a];
    if((a>=0xa00000&&a<=0xa01fff)||(a>=0xa04000&&a<=0xa04003))return bus_.m68kRead8(a);
    return 0xff;
}
uint16_t M68KDriver::read16(uint32_t a) const {return uint16_t((read8(a)<<8)|read8(a+1));}
void M68KDriver::write8(uint32_t a,uint8_t v) {bus_.m68kWrite8(a,v);}
void M68KDriver::write16(uint32_t a,uint16_t v) {write8(a,uint8_t(v>>8));write8(a+1,uint8_t(v));}
}
