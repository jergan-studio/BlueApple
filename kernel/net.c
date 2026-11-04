/* BlueApple's first IPv4 network layer: ARP, IPv4, ICMP and DHCP over E1000. */
#include "net.h"
#include "e1000.h"
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
#define ETH_LEN 14
#define IP_LEN 20
#define UDP_LEN 8
#define BOOTP_LEN 240
#define FRAME_MAX 1500
#define ETH_ARP 0x0806
#define ETH_IP 0x0800
#define IP_UDP 17
#define IP_ICMP 1
#define DHCP_CLIENT 68
#define DHCP_SERVER 67
static const u8 broadcast_mac[6]={255,255,255,255,255,255};
static u8 mac[6];
static u32 local_ip, offered_ip, server_ip, gateway_ip, subnet_mask, dns_ip;
static u16 ip_id=1;
static int configured, dhcp_state;
static u32 poll_count;
static u8 frame[FRAME_MAX];
static u16 rd16(const u8*p){return (u16)(((u16)p[0]<<8)|p[1]);}
static u32 rd32(const u8*p){return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];}
static void wr16(u8*p,u16 v){p[0]=(u8)(v>>8);p[1]=(u8)v;}
static void wr32(u8*p,u32 v){p[0]=(u8)(v>>24);p[1]=(u8)(v>>16);p[2]=(u8)(v>>8);p[3]=(u8)v;}
static u16 checksum(const u8*p,u32 n){u32 sum=0;while(n>1){sum+=rd16(p);p+=2;n-=2;}if(n)sum+=(u16)(p[0]<<8);while(sum>>16)sum=(sum&0xffff)+(sum>>16);return (u16)~sum;}
static void eth_header(u8*p,const u8*dst,u16 type){for(u32 i=0;i<6;i++){p[i]=dst[i];p[6+i]=mac[i];}wr16(p+12,type);}
static void send_dhcp(u8 type){
 u8 p[ETH_LEN+IP_LEN+UDP_LEN+BOOTP_LEN+64];u32 n=0;
 eth_header(p,broadcast_mac,ETH_IP);u8*ip=p+ETH_LEN;ip[0]=0x45;ip[1]=0;wr16(ip+2,IP_LEN+UDP_LEN+BOOTP_LEN+64);wr16(ip+4,ip_id++);wr16(ip+6,0x4000);ip[8]=64;ip[9]=IP_UDP;wr32(ip+12,0);wr32(ip+16,0xFFFFFFFFu);wr16(ip+10,0);wr16(ip+10,checksum(ip,IP_LEN));
 u8*udp=ip+IP_LEN;wr16(udp,DHCP_CLIENT);wr16(udp+2,DHCP_SERVER);wr16(udp+4,UDP_LEN+BOOTP_LEN+64);wr16(udp+6,0);
 u8*b=udp+UDP_LEN;for(u32 i=0;i<BOOTP_LEN+64;i++)b[i]=0;b[0]=1;b[1]=1;b[2]=6;wr32(b+4,0x4241504Cu);for(u32 i=0;i<6;i++)b[28+i]=mac[i];wr32(b+236,0x63825363);
 u32 o=240;b[o++]=53;b[o++]=1;b[o++]=type;
 if(type==3){b[o++]=50;b[o++]=4;wr32(b+o,offered_ip);o+=4;b[o++]=54;b[o++]=4;wr32(b+o,server_ip);o+=4;}
 b[o++]=55;b[o++]=3;b[o++]=1;b[o++]=3;b[o++]=6;b[o++]=255;
 wr16(udp+4,(u16)(UDP_LEN+o));wr16(ip+2,(u16)(IP_LEN+UDP_LEN+o));wr16(ip+10,0);wr16(ip+10,checksum(ip,IP_LEN));n=ETH_LEN+IP_LEN+UDP_LEN+o;(void)e1000_send(p,(u16)n);
}
static void send_arp_reply(const u8*dst,u32 target_ip){
 u8 p[42];eth_header(p,dst,ETH_ARP);wr16(p+14,1);wr16(p+16,ETH_IP);p[18]=6;p[19]=4;wr16(p+20,2);
 for(u32 i=0;i<6;i++){p[22+i]=mac[i];p[32+i]=dst[i];}wr32(p+28,local_ip);wr32(p+38,target_ip);(void)e1000_send(p,sizeof(p));
}
static void handle_arp(const u8*p,u16 len){
 if(len<42||rd16(p+14)!=1||rd16(p+16)!=ETH_IP||p[18]!=6||p[19]!=4)return;
 u32 target=rd32(p+38);if(rd16(p+20)==1&&target==local_ip)send_arp_reply(p+22,rd32(p+28));
}
static void handle_dhcp(const u8*p,u16 len){
 if(len<ETH_LEN+IP_LEN+UDP_LEN+BOOTP_LEN)return;const u8*ip=p+ETH_LEN;u16 ihl=(u16)((ip[0]&15)*4);if(ihl<20||len<ETH_LEN+ihl+UDP_LEN+BOOTP_LEN)return;
 const u8*udp=ip+ihl;if(rd16(udp+2)!=DHCP_CLIENT||rd16(udp+4)<UDP_LEN+BOOTP_LEN)return;const u8*b=udp+UDP_LEN;
 if(b[0]!=2||rd32(b+4)!=0x4241504Cu||rd32(b+236)!=0x63825363)return;
 offered_ip=rd32(b+16);server_ip=rd32(b+20);u8 msg=0;u32 mask=0,router=0,dns=0;u32 end=rd16(udp+4)-UDP_LEN;
 for(u32 i=240;i<end;){u8 tag=b[i++];if(tag==255)break;if(tag==0)continue;if(i>=end)break;u8 l=b[i++];if(i+l>end)break;
  if(tag==53&&l==1)msg=b[i];else if(tag==1&&l==4)mask=rd32(b+i);else if(tag==3&&l>=4)router=rd32(b+i);else if(tag==6&&l>=4)dns=rd32(b+i);else if(tag==54&&l==4)server_ip=rd32(b+i);i+=l;
 }
 if(msg==2&&dhcp_state==1){if(!offered_ip||!server_ip)return;dhcp_state=2;send_dhcp(3);}
 else if(msg==5&&dhcp_state==2){local_ip=offered_ip;subnet_mask=mask;gateway_ip=router;dns_ip=dns;configured=1;dhcp_state=3;}
}
static void handle_icmp(const u8*eth,const u8*ip,u16 ihl,u16 total){
 if(total<ihl+8||ip[9]!=IP_ICMP||rd32(ip+16)!=local_ip)return;const u8*in=ip+ihl;if(in[0]!=8)return;
 u16 data_len=(u16)(total-ihl);u8 out[FRAME_MAX];eth_header(out,eth+6,ETH_IP);u8*o=out+14;for(u32 i=0;i<ihl;i++)o[i]=ip[i];wr32(o+12,local_ip);wr32(o+16,rd32(ip+12));wr16(o+2,total);wr16(o+10,0);wr16(o+10,checksum(o,ihl));
 u8*icmp=o+ihl;for(u32 i=0;i<data_len;i++)icmp[i]=in[i];icmp[0]=0;wr16(icmp+2,0);wr16(icmp+2,checksum(icmp,data_len));(void)e1000_send(out,(u16)(14+total));
}
static void handle_ipv4(const u8*p,u16 len){
 if(len<34)return;const u8*ip=p+14;u16 ihl=(u16)((ip[0]&15)*4),total=rd16(ip+2);if(ihl<20||total<ihl||len<14+total||checksum(ip,ihl)!=0)return;
 if(ip[9]==IP_UDP){const u8*u=ip+ihl;if(total>=ihl+8&&rd16(u+2)==DHCP_CLIENT)handle_dhcp(p,len);}
 else if(ip[9]==IP_ICMP)handle_icmp(p,ip,ihl,total);
}
static void handle_frame(const u8*p,u16 len){if(len<14)return;u16 type=rd16(p+12);if(type==ETH_ARP)handle_arp(p,len);else if(type==ETH_IP)handle_ipv4(p,len);}
void net_initialize(void){
 configured=0;dhcp_state=0;local_ip=offered_ip=server_ip=gateway_ip=subnet_mask=dns_ip=0;
 (void)e1000_get_mac(mac);
}
void net_poll(void){
 if(!e1000_link_up())return;
 if(dhcp_state==0){send_dhcp(1);dhcp_state=1;poll_count=0;}
 if(dhcp_state==1||dhcp_state==2){if(++poll_count>5000000){poll_count=0;send_dhcp(dhcp_state==1?1:3);}}
 int n;while((n=e1000_receive(frame,sizeof(frame)))>0)handle_frame(frame,(u16)n);
}
int net_is_configured(void){return configured;}
u32 net_ipv4_address(void){return local_ip;}
