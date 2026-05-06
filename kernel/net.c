/* baSic_ - kernel/net.c
 * Copyright (C) 2026 Dhrubo
 * GPL v2 — see LICENSE
 *
 * ARP request/reply
 * IP + ICMP echo (ping)
 */
#include "net.h"
#include "e1000.h"
#include "timer.h"
#include "../lib/string.h"
#include "../lib/kprintf.h"

static u8 my_mac[6];
static u8 my_ip[4]  = { MY_IP0, MY_IP1, MY_IP2, MY_IP3 };
static u8 bcast[6]  = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/* ARP cache: 8 entries */
#define ARP_CACHE_SIZE  8
typedef struct {
    u8 ip[4];
    u8 mac[6];
    u8 valid;
} arp_entry_t;
static arp_entry_t arp_cache[ARP_CACHE_SIZE];

static u16 htons(u16 v) { return (u16)((v >> 8) | (v << 8)); }

static u16 ip_checksum(void *data, u32 len)
{
    u16 *p = (u16 *)data;
    u32  sum = 0;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(u8 *)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (u16)(~sum);
}

static void arp_cache_set(u8 ip[4], u8 mac[6])
{

    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid &&
            memcmp(arp_cache[i].ip, ip, 4) == 0) {
            memcpy(arp_cache[i].mac, mac, 6);
            return;
        }
    }

    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) {
            memcpy(arp_cache[i].ip,  ip,  4);
            memcpy(arp_cache[i].mac, mac, 6);
            arp_cache[i].valid = 1;
            return;
        }
    }
}

static u8 *arp_cache_get(u8 ip[4])
{
    for (int i = 0; i < ARP_CACHE_SIZE; i++)
        if (arp_cache[i].valid && memcmp(arp_cache[i].ip, ip, 4) == 0)
            return arp_cache[i].mac;
    return NULL;
}

static void handle_arp(u8 *pkt, u16 len)
{
    (void)len;
    eth_hdr_t *eth = (eth_hdr_t *)pkt;
    arp_pkt_t *arp = (arp_pkt_t *)(pkt + sizeof(eth_hdr_t));

    /*  sender */
    arp_cache_set(arp->spa, arp->sha);

    if (htons(arp->op) != ARP_REQUEST) return;
    if (memcmp(arp->tpa, my_ip, 4) != 0) return;

    /* send replies */
    u8 reply[sizeof(eth_hdr_t) + sizeof(arp_pkt_t)];
    memset(reply, 0, sizeof(reply));

    eth_hdr_t *reth = (eth_hdr_t *)reply;
    memcpy(reth->dst, eth->src, 6);
    memcpy(reth->src, my_mac, 6);
    reth->type = htons(ETH_TYPE_ARP);

    arp_pkt_t *rarp = (arp_pkt_t *)(reply + sizeof(eth_hdr_t));
    rarp->htype = htons(1);
    rarp->ptype = htons(ETH_TYPE_IP);
    rarp->hlen  = 6;
    rarp->plen  = 4;
    rarp->op    = htons(ARP_REPLY);
    memcpy(rarp->sha, my_mac, 6);
    memcpy(rarp->spa, my_ip,  4);
    memcpy(rarp->tha, arp->sha, 6);
    memcpy(rarp->tpa, arp->spa, 4);

    e1000_send(reply, sizeof(reply));
    kprintf("[net] ARP reply sent\n");
}

static void handle_icmp(u8 *pkt, u16 len)
{
    eth_hdr_t  *eth  = (eth_hdr_t *)pkt;
    ip_hdr_t   *ip   = (ip_hdr_t  *)(pkt + sizeof(eth_hdr_t));
    icmp_hdr_t *icmp = (icmp_hdr_t *)(pkt + sizeof(eth_hdr_t) + sizeof(ip_hdr_t));

    if (icmp->type != ICMP_ECHO_REQ) return;

    kprintf("[net] ping from %d.%d.%d.%d\n",
            ip->src[0], ip->src[1], ip->src[2], ip->src[3]);

    u8 reply[1500];
    if (len > 1500) len = 1500;
    memcpy(reply, pkt, len);

    eth_hdr_t  *reth  = (eth_hdr_t *)reply;
    ip_hdr_t   *rip   = (ip_hdr_t  *)(reply + sizeof(eth_hdr_t));
    icmp_hdr_t *ricmp = (icmp_hdr_t *)(reply + sizeof(eth_hdr_t) + sizeof(ip_hdr_t));

    memcpy(reth->dst, eth->src, 6);
    memcpy(reth->src, my_mac,   6);

    memcpy(rip->dst, ip->src, 4);
    memcpy(rip->src, my_ip,   4);
    rip->checksum = 0;
    rip->checksum = ip_checksum(rip, sizeof(ip_hdr_t));

    ricmp->type     = ICMP_ECHO_REP;
    ricmp->checksum = 0;
    u32 icmp_len = len - sizeof(eth_hdr_t) - sizeof(ip_hdr_t);
    ricmp->checksum = ip_checksum(ricmp, icmp_len);

    e1000_send(reply, len);
    kprintf("[net] ping reply sent\n");
}

static void handle_ip(u8 *pkt, u16 len)
{
    ip_hdr_t *ip = (ip_hdr_t *)(pkt + sizeof(eth_hdr_t));
    if (memcmp(ip->dst, my_ip, 4) != 0) return;
    if (ip->proto == IP_PROTO_ICMP) handle_icmp(pkt, len);
}

void net_poll(void)
{
    u8  buf[1500];
    int n;
    while ((n = e1000_recv(buf, sizeof(buf))) > 0) {
        eth_hdr_t *eth = (eth_hdr_t *)buf;
        u16 type = htons(eth->type);
        if (type == ETH_TYPE_ARP) handle_arp(buf, (u16)n);
        else if (type == ETH_TYPE_IP) handle_ip(buf, (u16)n);
    }
}

int net_arp_request(u8 ip[4])
{
    u8 pkt[sizeof(eth_hdr_t) + sizeof(arp_pkt_t)];
    memset(pkt, 0, sizeof(pkt));

    eth_hdr_t *eth = (eth_hdr_t *)pkt;
    memcpy(eth->dst, bcast,  6);
    memcpy(eth->src, my_mac, 6);
    eth->type = htons(ETH_TYPE_ARP);

    arp_pkt_t *arp = (arp_pkt_t *)(pkt + sizeof(eth_hdr_t));
    arp->htype = htons(1);
    arp->ptype = htons(ETH_TYPE_IP);
    arp->hlen  = 6;
    arp->plen  = 4;
    arp->op    = htons(ARP_REQUEST);
    memcpy(arp->sha, my_mac, 6);
    memcpy(arp->spa, my_ip,  4);
    memset(arp->tha, 0, 6);
    memcpy(arp->tpa, ip, 4);

    e1000_send(pkt, sizeof(pkt));
    kprintf("[net] ARP request -> %d.%d.%d.%d\n",
            ip[0], ip[1], ip[2], ip[3]);

    
    for (int i = 0; i < 50; i++) {
        net_poll();
        u8 *mac = arp_cache_get(ip);
        if (mac) {
            kprintf("[net] ARP resolved: %d:%d:%d:%d:%d:%d\n",
                    mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
            return 1;
        }
        timer_sleep(10);
    }
    kprintf("[net] ARP timeout\n");
    return 0;
}

int net_ping(u8 ip[4])
{
    u8 *dst_mac = arp_cache_get(ip);
    if (!dst_mac) {
        if (!net_arp_request(ip)) return -1;
        dst_mac = arp_cache_get(ip);
        if (!dst_mac) return -1;
    }

    u8 pkt[sizeof(eth_hdr_t) + sizeof(ip_hdr_t) + sizeof(icmp_hdr_t) + 32];
    memset(pkt, 0, sizeof(pkt));

    eth_hdr_t  *eth  = (eth_hdr_t *)pkt;
    ip_hdr_t   *iph  = (ip_hdr_t  *)(pkt + sizeof(eth_hdr_t));
    icmp_hdr_t *icmp = (icmp_hdr_t *)(pkt + sizeof(eth_hdr_t) + sizeof(ip_hdr_t));

    memcpy(eth->dst, dst_mac, 6);
    memcpy(eth->src, my_mac,  6);
    eth->type = htons(ETH_TYPE_IP);

    iph->ihl_ver   = 0x45;
    iph->ttl       = 64;
    iph->proto     = IP_PROTO_ICMP;
    iph->total_len = htons(sizeof(ip_hdr_t) + sizeof(icmp_hdr_t) + 32);
    memcpy(iph->src, my_ip, 4);
    memcpy(iph->dst, ip,    4);
    iph->checksum  = ip_checksum(iph, sizeof(ip_hdr_t));

    icmp->type     = ICMP_ECHO_REQ;
    icmp->id       = htons(0x1337);
    icmp->seq      = htons(1);
    icmp->checksum = ip_checksum(icmp, sizeof(icmp_hdr_t) + 32);

    e1000_send(pkt, sizeof(pkt));
    kprintf("[net] ping sent to %d.%d.%d.%d\n",
            ip[0], ip[1], ip[2], ip[3]);

    for (int i = 0; i < 100; i++) {
        net_poll();
        timer_sleep(10);
    }
    return 0;
}

void net_init(void)
{
    memset(arp_cache, 0, sizeof(arp_cache));
    e1000_get_mac(my_mac);
    kprintf("[OK] net: stack ready — IP %d.%d.%d.%d\n",
            my_ip[0], my_ip[1], my_ip[2], my_ip[3]);
}