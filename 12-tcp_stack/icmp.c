#include "icmp.h"
#include "ip.h"
#include "rtable.h"
#include "arp.h"
#include "base.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// send icmp packet
void icmp_send_packet(const char *in_pkt, int len, u8 type, u8 code)
{
	// fprintf(stderr, "TODO: malloc and send icmp packet.\n");

	struct iphdr *in_ip = packet_to_ip_hdr(in_pkt);
	int packet_len;

	// 计算新包的总长度
	if (type == ICMP_ECHOREPLY) {
		// Echo Reply 的长度通常与 Request 一致
		packet_len = len;
	} else {
		// 错误报文长度：Ethernet头 + IP头 + ICMP头 + 原IP头 + 原IP数据前8字节
		packet_len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + ICMP_HDR_SIZE + IP_HDR_SIZE(in_ip) + 8;
	}

	// 申请内存
	char *packet = malloc(packet_len);
	if (!packet) return;

	struct ether_header *eth = (struct ether_header *)packet;
	struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct icmphdr *icmp = (struct icmphdr *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);

	// 1. 填充 Ethernet 头 (Type 设为 IP，MAC 由 ARP 层处理)
	eth->ether_type = htons(ETH_P_IP);

	// 2. 填充 IP 头
	u32 saddr = 0;
	u32 daddr = ntohl(in_ip->saddr);

	if (type == ICMP_ECHOREPLY) {
		// 如果是回复 Ping，源 IP 是请求的目的 IP
		saddr = ntohl(in_ip->daddr);
	} else {
		// 如果是报错，源 IP 暂时填 0，由 ip_send_packet 根据出口接口自动填充
		saddr = 0;
	}

	ip_init_hdr(ip, saddr, daddr, packet_len - ETHER_HDR_SIZE, IPPROTO_ICMP);

	// 3. 填充 ICMP 头和数据
	icmp->type = type;
	icmp->code = code;

	if (type == ICMP_ECHOREPLY) {
		// Echo Reply: 复制 Identifier, Sequence 和 Payload
		struct icmphdr *in_icmp = (struct icmphdr *)((char *)in_ip + IP_HDR_SIZE(in_ip));
		icmp->icmp_identifier = in_icmp->icmp_identifier;
		icmp->icmp_sequence = in_icmp->icmp_sequence;
		
		char *payload = (char *)icmp + ICMP_HDR_SIZE;
		char *in_payload = (char *)in_icmp + ICMP_HDR_SIZE;
		// 剩余数据长度 = 总长 - Eth头 - IP头 - ICMP头
		int payload_len = len - (ETHER_HDR_SIZE + IP_HDR_SIZE(in_ip) + ICMP_HDR_SIZE);
		if (payload_len > 0)
			memcpy(payload, in_payload, payload_len);
	} else {
		// 错误报文: Identifier/Sequence 未使用(填0)，Payload 是原 IP 头 + 8字节
		icmp->icmp_identifier = 0;
		icmp->icmp_sequence = 0;
		
		char *payload = (char *)icmp + ICMP_HDR_SIZE;
		memcpy(payload, in_ip, IP_HDR_SIZE(in_ip) + 8);
	}

	// 4. 计算 ICMP 校验和
	icmp->checksum = icmp_checksum(icmp, packet_len - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE);

	// 5. 发送
	ip_send_packet(packet, packet_len);
}