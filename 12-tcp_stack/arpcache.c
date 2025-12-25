#include "arpcache.h"
#include "arp.h"
#include "ether.h"
#include "icmp.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static arpcache_t arpcache;

// 初始化 ARP 缓存
void arpcache_init()
{
    bzero(&arpcache, sizeof(arpcache_t));
    init_list_head(&(arpcache.req_list));
    pthread_mutex_init(&arpcache.lock, NULL);
    // 启动清理线程
    pthread_create(&arpcache.thread, NULL, arpcache_sweep, NULL);
}

// 销毁 ARP 缓存
void arpcache_destroy()
{
    pthread_mutex_lock(&arpcache.lock);

    struct arp_req *req_entry = NULL, *req_q;
    list_for_each_entry_safe(req_entry, req_q, &(arpcache.req_list), list) {
        struct cached_pkt *pkt_entry = NULL, *pkt_q;
        list_for_each_entry_safe(pkt_entry, pkt_q, &(req_entry->cached_packets), list) {
            list_delete_entry(&(pkt_entry->list));
            free(pkt_entry->packet);
            free(pkt_entry);
        }

        list_delete_entry(&(req_entry->list));
        free(req_entry);
    }

    pthread_kill(arpcache.thread, SIGTERM);
    pthread_mutex_unlock(&arpcache.lock);
}

// 查找 IP->MAC 映射
// 返回 1 找到，0 未找到
int arpcache_lookup(u32 ip4, u8 mac[ETH_ALEN])
{
    pthread_mutex_lock(&arpcache.lock);
    
    for(int i = 0; i < MAX_ARP_SIZE; i++) {
        // 必须是有效条目且 IP 匹配
        if(arpcache.entries[i].valid && arpcache.entries[i].ip4 == ip4) {
            memcpy(mac, arpcache.entries[i].mac, ETH_ALEN);
            pthread_mutex_unlock(&arpcache.lock);
            return 1;
        }
    }

    pthread_mutex_unlock(&arpcache.lock);
    return 0;
}

// 将数据包挂起（等待 ARP 解析）
void arpcache_append_packet(iface_info_t *iface, u32 ip4, char *packet, int len)
{
    pthread_mutex_lock(&arpcache.lock);

    struct arp_req *req_entry = NULL;
    int found = 0;

    // 1. 检查是否已经有针对该 IP 的请求在等待中
    list_for_each_entry(req_entry, &arpcache.req_list, list) {
        if(req_entry->ip4 == ip4) {
            found = 1;
            break;
        }
    }

    // 2. 如果没找到，创建一个新的请求条目
    if(!found) {
        req_entry = malloc(sizeof(struct arp_req));
        init_list_head(&req_entry->list);
        init_list_head(&req_entry->cached_packets);
        
        req_entry->iface = iface;
        req_entry->ip4 = ip4;
        req_entry->sent = time(NULL);
        req_entry->retries = 1; // 第一次尝试
        
        list_add_tail(&req_entry->list, &arpcache.req_list);

        // 新建请求时，立即发送第一个 ARP Request
        arp_send_request(iface, ip4);
    }

    // 3. 将数据包加入到该请求的缓冲队列中
    struct cached_pkt *pkt_entry = malloc(sizeof(struct cached_pkt));
    pkt_entry->packet = packet;
    pkt_entry->len = len;
    init_list_head(&pkt_entry->list);
    
    list_add_tail(&pkt_entry->list, &req_entry->cached_packets);

    pthread_mutex_unlock(&arpcache.lock);
}

// 插入 IP->MAC 映射，并发送等待的包
void arpcache_insert(u32 ip4, u8 mac[ETH_ALEN])
{
    pthread_mutex_lock(&arpcache.lock);

    // --- Part 1: 更新缓存 ---
    int idx = -1;
    // 找已存在的条目或空位
    for(int i = 0; i < MAX_ARP_SIZE; i++) {
        if(!arpcache.entries[i].valid) {
            if(idx == -1) idx = i; // 记录第一个空位
        } 
        else if(arpcache.entries[i].ip4 == ip4) {
            idx = i; // 找到匹配项
            break;
        }
    }

    // 如果没空位且没匹配，随机覆盖一个（简单策略：覆盖最后一个）
    if(idx == -1) idx = 0; 

    // 写入缓存
    arpcache.entries[idx].ip4 = ip4;
    memcpy(arpcache.entries[idx].mac, mac, ETH_ALEN);
    arpcache.entries[idx].added = time(NULL);
    arpcache.entries[idx].valid = 1;

    // --- Part 2: 发送等待队列中的包 ---
    struct arp_req *req, *req_q;
    list_for_each_entry_safe(req, req_q, &arpcache.req_list, list) {
        if(req->ip4 == ip4) {
            // 找到了等待该 IP 的请求列表
            struct cached_pkt *pkt, *pkt_q;
            list_for_each_entry_safe(pkt, pkt_q, &req->cached_packets, list) {
                
                // 填充以太网头部的目的 MAC
                struct ether_header *eh = (struct ether_header *)(pkt->packet);
                memcpy(eh->ether_dhost, mac, ETH_ALEN);
                
                // 发送数据包
                iface_send_packet(req->iface, pkt->packet, pkt->len);
                
                // 移除并释放缓存包节点
                list_delete_entry(&pkt->list);
                free(pkt); 
                // 注意：packet 内存由 iface_send_packet 负责释放（或它发送完我们可以不管，
                // 但根据框架 device_internal.c 的 iface_send_packet 实现，它最后调用了 free(packet)。
                // 所以这里我们不需要 free(pkt->packet)）
            }
            // 移除请求节点
            list_delete_entry(&req->list);
            free(req);
        }
    }

    pthread_mutex_unlock(&arpcache.lock);
}

// 定期清理和重传线程
void *arpcache_sweep(void *arg) 
{
    while (1) {
        sleep(1); // 每秒执行一次
        pthread_mutex_lock(&arpcache.lock);
        
        time_t now = time(NULL);

        // 1. 清理过期条目 ( > 15秒 )
        for(int i = 0; i < MAX_ARP_SIZE; i++) {
            if(arpcache.entries[i].valid && (now - arpcache.entries[i].added > ARP_ENTRY_TIMEOUT)) {
                arpcache.entries[i].valid = 0;
            }
        }

        // 2. 检查请求列表：重传或超时
        struct arp_req *req, *req_q;
        list_for_each_entry_safe(req, req_q, &arpcache.req_list, list) {
            // 如果距离上次发送超过 1 秒
            if(now - req->sent > 1) {
                req->retries++;
                
                if(req->retries > ARP_REQUEST_MAX_RETRIES) {
                    // 重试超过 5 次：视为 Host Unreachable
                    struct cached_pkt *pkt, *pkt_q;
                    list_for_each_entry_safe(pkt, pkt_q, &req->cached_packets, list) {
                        // 发送 ICMP Host Unreachable
                        pthread_mutex_unlock(&arpcache.lock); // icmp_send可能耗时，稍微解锁一下防止死锁（可选）
                        icmp_send_packet(pkt->packet, pkt->len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
                        pthread_mutex_lock(&arpcache.lock);

                        // 释放包内存
                        list_delete_entry(&pkt->list);
                        free(pkt->packet); // icmp_send_packet 会拷贝数据，原包这里要释放
                        free(pkt);
                    }
                    // 移除请求节点
                    list_delete_entry(&req->list);
                    free(req);
                } 
                else {
                    // 继续重试：更新时间，发送 ARP Request
                    req->sent = now;
                    arp_send_request(req->iface, req->ip4);
                }
            }
        }

        pthread_mutex_unlock(&arpcache.lock);
    }
    return NULL;
}
