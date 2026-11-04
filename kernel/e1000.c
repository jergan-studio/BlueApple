/* Polling QEMU 82540EM / Intel 82540EM-compatible E1000 PCI driver.
 * The current kernel has no paging: descriptor rings and buffers are DMA-visible
 * at their identity-mapped physical addresses. Interrupts are deliberately masked.
 */
#include "e1000.h"
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
#define PCI_ADDR 0xCF8
#define PCI_DATA 0xCFC
#define VENDOR 0x8086
#define DEVICE 0x100E
#define RX_COUNT 8
#define TX_COUNT 8
#define BUF_SIZE 2048
enum {
 CTRL=0x0000, STATUS=0x0008, ICR=0x00C0, IMC=0x00D8,
 RCTL=0x0100, TCTL=0x0400, TIPG=0x0410,
 RDBAL=0x2800, RDBAH=0x2804, RDLEN=0x2808, RDH=0x2810, RDT=0x2818,
 TDBAL=0x3800, TDBAH=0x3804, TDLEN=0x3808, TDH=0x3810, TDT=0x3818,
 RAL=0x5400, RAH=0x5404
};
typedef struct __attribute__((packed)) { u32 addr, addr_hi; u16 length, checksum; u8 status, errors; u16 special; } rx_desc_t;
typedef struct __attribute__((packed)) { u32 addr, addr_hi; u16 length; u8 cso, cmd, status, css; u16 special; } tx_desc_t;
static volatile u32* mmio;
static rx_desc_t rx_ring[RX_COUNT] __attribute__((aligned(16)));
static tx_desc_t tx_ring[TX_COUNT] __attribute__((aligned(16)));
static u8 rx_buffers[RX_COUNT][BUF_SIZE] __attribute__((aligned(16)));
static u8 tx_buffers[TX_COUNT][BUF_SIZE] __attribute__((aligned(16)));
static u32 rx_next, tx_next;
static int initialized;
static inline void outl(u16 p,u32 v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static inline u32 inl(u16 p){u32 v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}
static u32 pci_read(u8 b,u8 d,u8 f,u8 r){
 outl(PCI_ADDR,0x80000000u|((u32)b<<16)|((u32)d<<11)|((u32)f<<8)|(r&0xFC));return inl(PCI_DATA);
}
static void pci_write(u8 b,u8 d,u8 f,u8 r,u32 v){
 outl(PCI_ADDR,0x80000000u|((u32)b<<16)|((u32)d<<11)|((u32)f<<8)|(r&0xFC));outl(PCI_DATA,v);
}
static u32 rr(u32 r){return mmio[r/4];}
static void rw(u32 r,u32 v){mmio[r/4]=v;}
static void delay(void){for(volatile u32 i=0;i<10000;i++)__asm__ volatile("pause");}
static int find_device(void){
 for(u32 b=0;b<256;b++)for(u32 d=0;d<32;d++){
  u32 id=pci_read((u8)b,(u8)d,0,0);
  if((id&0xFFFF)==VENDOR&&(id>>16)==DEVICE){
   u32 cmd=pci_read((u8)b,(u8)d,0,4);pci_write((u8)b,(u8)d,0,4,cmd|6u);
   u32 bar=pci_read((u8)b,(u8)d,0,0x10);
   if((bar&1u)||!(bar&0xFFFFFFF0u))return 0;
   mmio=(volatile u32*)(bar&0xFFFFFFF0u);return 1;
  }
 }
 return 0;
}
int e1000_initialize(void){
 initialized=0;mmio=0;if(!find_device())return 0;
 rw(IMC,0xFFFFFFFFu);(void)rr(ICR);rw(CTRL,rr(CTRL)|(1u<<26));delay();
 rw(IMC,0xFFFFFFFFu);(void)rr(ICR);
 u32 ral=rr(RAL),rah=rr(RAH);if(!(rah&0x80000000u)||(!ral&&!(rah&0xFFFFu)))return 0;
 for(u32 i=0;i<RX_COUNT;i++){
  rx_ring[i].addr=(u32)rx_buffers[i];rx_ring[i].addr_hi=0;rx_ring[i].length=0;
  rx_ring[i].checksum=0;rx_ring[i].status=0;rx_ring[i].errors=0;rx_ring[i].special=0;
 }
 for(u32 i=0;i<TX_COUNT;i++){
  tx_ring[i].addr=(u32)tx_buffers[i];tx_ring[i].addr_hi=0;tx_ring[i].length=0;
  tx_ring[i].cso=0;tx_ring[i].cmd=0;tx_ring[i].status=1;tx_ring[i].css=0;tx_ring[i].special=0;
 }
 rx_next=tx_next=0;
 rw(RDBAL,(u32)rx_ring);rw(RDBAH,0);rw(RDLEN,sizeof(rx_ring));rw(RDH,0);rw(RDT,RX_COUNT-1);
 rw(TDBAL,(u32)tx_ring);rw(TDBAH,0);rw(TDLEN,sizeof(tx_ring));rw(TDH,0);rw(TDT,0);
 rw(TIPG,0x0060200A);rw(RCTL,(1u<<1)|(1u<<2)|(1u<<4)|(1u<<15)|(1u<<26));
 rw(TCTL,(1u<<1)|(1u<<3)|(0x10u<<4)|(0x40u<<12));
 initialized=1;return 1;
}
int e1000_link_up(void){return initialized&&((rr(STATUS)&2u)!=0);}
int e1000_get_mac(u8 out[6]){
 if(!initialized||!out)return 0;
 u32 low=rr(RAL),high=rr(RAH);
 if(!(high&0x80000000u))return 0;
 out[0]=(u8)low;out[1]=(u8)(low>>8);out[2]=(u8)(low>>16);out[3]=(u8)(low>>24);
 out[4]=(u8)high;out[5]=(u8)(high>>8);return 1;
}
int e1000_send(const void* packet,u16 length){
 if(!initialized||!packet||length<14||length>BUF_SIZE)return 0;
 u32 i=tx_next;if(!(tx_ring[i].status&1u))return 0;
 const u8* src=(const u8*)packet;for(u32 n=0;n<length;n++)tx_buffers[i][n]=src[n];
 tx_ring[i].length=length;tx_ring[i].cmd=0x0B;tx_ring[i].status=0;
 __asm__ volatile("":::"memory");rw(TDT,(i+1)%TX_COUNT);tx_next=(i+1)%TX_COUNT;return 1;
}
int e1000_receive(u8* packet,u16 capacity){
 if(!initialized||!packet||!capacity)return 0;
 u32 i=rx_next;if(!(rx_ring[i].status&1u))return 0;
 u16 len=rx_ring[i].length;if(len>capacity)len=capacity;
 for(u32 n=0;n<len;n++)packet[n]=rx_buffers[i][n];
 rx_ring[i].status=0;__asm__ volatile("":::"memory");rw(RDT,i);rx_next=(i+1)%RX_COUNT;return (int)len;
}
void e1000_poll(void){static u8 frame[BUF_SIZE];while(e1000_receive(frame,sizeof(frame))>0){}}
