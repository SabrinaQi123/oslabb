#include "ip.h"
#include "icmp.h"
#include "arpcache.h"
#include "rtable.h"
#include "arp.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>


void ip_init_hdr(struct iphdr *ip, u32 saddr, u32 daddr, u16 len, u8 proto)
{
    ip->version = 4;
    ip->ihl = 5;
    ip->tos = 0;
    ip->tot_len = htons(len);
    ip->id = rand();
    ip->frag_off = htons(IP_DF);
    ip->ttl = DEFAULT_TTL;
    ip->protocol = proto;
    ip->saddr = htonl(saddr);
    ip->daddr = htonl(daddr);
    ip->checksum = ip_checksum(ip);
}

// 查找最长前缀匹配 
rt_entry_t *longest_prefix_match(u32 dst)
{
    rt_entry_t *res = NULL;
    rt_entry_t *entry;
    u32 mmask = 0;

    // 1. 第一轮遍历：寻找匹配且掩码最长的特定路由
    list_for_each_entry(entry, &rtable, list){
        
        if((dst & entry->mask) == (entry->dest & entry->mask)){
            if(entry->mask > mmask){
                mmask = entry->mask;
                res = entry;
            }
        }
    }
    
    
   
    if(!res){
        list_for_each_entry(entry, &rtable, list){
            if(entry->dest == 0 && entry->mask == 0){
                res = entry;
                break;
            }
        }
    }
    
    if(!res) log(DEBUG, "ip: dst doesn't match -- no default route\n");
    return res;
}

// 发送 IP 数据包 
void ip_send_packet(char *packet, int len)
{
    struct iphdr *ih = packet_to_ip_hdr(packet);
    u32 dip = ntohl(ih->daddr);
    
    // 1. 查路由：决定从哪个口出去
    rt_entry_t *re = longest_prefix_match(dip);
    if(!re) {
        log(ERROR, "ip_send_packet: no route to host %x, drop packet.\n", dip); 
        free(packet); 
        return;
    }
    
    
    if (ih->saddr == 0) {
        ih->saddr = htonl(re->iface->ip);
        
        ih->checksum = ip_checksum(ih); 
    }
    
    
    u32 gateway = re->gw ? re->gw : dip;
    
    
    iface_send_packet_by_arp(re->iface, gateway, packet, len);
}