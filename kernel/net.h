/* baSic_ - kernel/net.h
 * Copyright (C) 2026 Dhrubo
 * GPL v2 — see LICENSE
 *
 * network stack: ethernet, ARP, IP, ICMP
 */
#ifndef NET_H
#define NET_H
#include "../include/types.h"

/* ethernet part */
#define ETH_ALEN       6
#define ETH_TYPE_ARP   0x0806
#define ETH_TYPE_IP    0x0800

/* ARP part */
#define ARP_REQUEST    1
#define ARP_REPLY      2

/* IP protocols.. */
#define IP_PROTO_ICMP  1
#define IP_PROTO_UDP   17

/* ICMP types. */
#define ICMP_ECHO_REQ  8
#define ICMP_ECHO_REP  0

typedef struct __attribute__((packed)) {
    u8  dst[ETH_ALEN];
    u8  src[ETH_ALEN];
    u16 type;
} eth_hdr_t;

typedef struct __attribute__((packed)) {
    u16 htype;
    u16 ptype;
    u8  hlen;
    u8  plen;
    u16 op;
    u8  sha[ETH_ALEN];
    u8  spa[4];
    u8  tha[ETH_ALEN];
    u8  tpa[4];
} arp_pkt_t;

typedef struct __attribute__((packed)) {
    u8  ihl_ver;
    u8  tos;
    u16 total_len;
    u16 id;
    u16 flags_frag;
    u8  ttl;
    u8  proto;
    u16 checksum;
    u8  src[4];
    u8  dst[4];
} ip_hdr_t;

typedef struct __attribute__((packed)) {
    u8  type;
    u8  code;
    u16 checksum;
    u16 id;
    u16 seq;
} icmp_hdr_t;

/* 10.0.2.15 */
#define MY_IP0  10
#define MY_IP1   0
#define MY_IP2   2
#define MY_IP3  15

#define GW_IP0  10
#define GW_IP1   0
#define GW_IP2   2
#define GW_IP3   2

void net_init(void);
void net_poll(void);
int  net_arp_request(u8 ip[4]);
int  net_ping(u8 ip[4]);

#endif