#include "arp.h"
#include "base.h"
#include "types.h"
#include "ether.h"
#include "arpcache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h" // 引入日志打印，方便调试

// 发送 ARP 请求
// 构造一个广播包，询问 dst_ip 的 MAC 地址
void arp_send_request(iface_info_t *iface, u32 dst_ip)
{
    // 1. 分配内存：以太网头 + ARP 头
    int packet_len = ETHER_HDR_SIZE + sizeof(struct ether_arp);
    char *packet = malloc(packet_len);
    if (!packet) {
        log(ERROR, "malloc failed in arp_send_request");
        return;
    }

    struct ether_header *eth = (struct ether_header *)packet;
    struct ether_arp *arp = (struct ether_arp *)(packet + ETHER_HDR_SIZE);

    // 2. 填充以太网头
    memset(eth->ether_dhost, 0xff, ETH_ALEN); // 目的 MAC：全 FF (广播)
    memcpy(eth->ether_shost, iface->mac, ETH_ALEN); // 源 MAC：本接口 MAC
    eth->ether_type = htons(ETH_P_ARP);

    // 3. 填充 ARP 头
    arp->arp_hrd = htons(ARPHRD_ETHER); // 硬件类型：以太网
    arp->arp_pro = htons(ETH_P_IP);     // 协议类型：IP
    arp->arp_hln = ETH_ALEN;            // 硬件地址长度：6
    arp->arp_pln = 4;                   // 协议地址长度：4 (IPv4)
    arp->arp_op = htons(ARPOP_REQUEST); // 操作码：请求 (1)

    memcpy(arp->arp_sha, iface->mac, ETH_ALEN); // 发送者 MAC
    arp->arp_spa = htonl(iface->ip);            // 发送者 IP
    memset(arp->arp_tha, 0, ETH_ALEN);          // 目标 MAC：未知，填 0
    arp->arp_tpa = htonl(dst_ip);               // 目标 IP

    // 4. 发送
    iface_send_packet(iface, packet, packet_len);
    // log(DEBUG, "Sent ARP Request for %x", dst_ip);
}

// 发送 ARP 回复

void arp_send_reply(iface_info_t *iface, struct ether_arp *req_hdr)
{
    int packet_len = ETHER_HDR_SIZE + sizeof(struct ether_arp);
    char *packet = malloc(packet_len);
    if (!packet) return;

    struct ether_header *eth = (struct ether_header *)packet;
    struct ether_arp *arp = (struct ether_arp *)(packet + ETHER_HDR_SIZE);

    // 1. 以太网头
    memcpy(eth->ether_dhost, req_hdr->arp_sha, ETH_ALEN); // 回复给请求者
    memcpy(eth->ether_shost, iface->mac, ETH_ALEN);
    eth->ether_type = htons(ETH_P_ARP);

    // 2. ARP 头
    arp->arp_hrd = htons(ARPHRD_ETHER);
    arp->arp_pro = htons(ETH_P_IP);
    arp->arp_hln = ETH_ALEN;
    arp->arp_pln = 4;
    arp->arp_op = htons(ARPOP_REPLY); // 操作码：回复 (2)

    memcpy(arp->arp_sha, iface->mac, ETH_ALEN);
    arp->arp_spa = htonl(iface->ip);
    memcpy(arp->arp_tha, req_hdr->arp_sha, ETH_ALEN);
    arp->arp_tpa = req_hdr->arp_spa;

    iface_send_packet(iface, packet, packet_len);
    // log(DEBUG, "Sent ARP Reply to %x", ntohl(req_hdr->arp_spa));
}

// 处理收到的 ARP 包
void handle_arp_packet(iface_info_t *iface, char *packet, int len)
{
    struct ether_arp *arp = (struct ether_arp *)(packet + ETHER_HDR_SIZE);
    
    // 安全检查：长度是否足够
    if (len < ETHER_HDR_SIZE + sizeof(struct ether_arp)) {
        free(packet);
        return;
    }

    u32 sip = ntohl(arp->arp_spa); // 发送者 IP
    u32 tip = ntohl(arp->arp_tpa); // 目标 IP (即被请求的 IP)
    u16 op = ntohs(arp->arp_op);

    // 逻辑参照学长报告 Figure 2
    if (op == ARPOP_REQUEST) {
        // 如果是问我的 IP，回复它
        if (tip == iface->ip) {
            // log(DEBUG, "Received ARP Request for me from %x", sip);
            arp_send_reply(iface, arp);
        }
        // 优化：无论是不是问我，既然收到广播，顺便把对方的 MAC 记下来
        arpcache_insert(sip, arp->arp_sha);
    } 
    else if (op == ARPOP_REPLY) {
        // 如果是给我的回复 (或者我监听到的)
        if (tip == iface->ip) {
            // log(DEBUG, "Received ARP Reply from %x", sip);
            // 插入缓存，这会自动触发发送等待队列中的包
            arpcache_insert(sip, arp->arp_sha);
        }
    }

    
    free(packet);
}

// 查 ARP 并发送 
void iface_send_packet_by_arp(iface_info_t *iface, u32 dst_ip, char *packet, int len)
{
    struct ether_header *eh = (struct ether_header *)packet;
    
    // 1. 填写以太网头部的源 MAC 
    memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
    eh->ether_type = htons(ETH_P_IP);

    u8 dst_mac[ETH_ALEN];
    
    // 2. 查缓存
    int found = arpcache_lookup(dst_ip, dst_mac);
    
    if (found) {
        // A. 找到了：填目的 MAC，直接发
        memcpy(eh->ether_dhost, dst_mac, ETH_ALEN);
        iface_send_packet(iface, packet, len);
    }
    else {
        // B. 没找到：挂起包，发起 ARP 请求
        // log(DEBUG, "ARP miss for %x, pending packet", dst_ip);
        arpcache_append_packet(iface, dst_ip, packet, len);
    }
}