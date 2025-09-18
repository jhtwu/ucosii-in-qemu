#include "net/net.h"
#include "hw/virtio_net.h"
#include "hw/serial.h"
#include "ucos_ii.h"

#define ETH_TYPE_ARP 0x0806
#define ETH_TYPE_IP  0x0800
#define ARP_HTYPE_ETH 0x0001
#define ARP_PTYPE_IP  0x0800
#define ARP_OP_REQUEST 0x0001
#define ARP_OP_REPLY   0x0002
#define IP_PROTO_ICMP 1

static const uint8_t NET_MAC[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
static const uint8_t NET_IP[4]  = {192, 168, 1, 1};

struct eth_hdr {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t type;
} __attribute__((packed));

struct arp_pkt {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    uint8_t sha[6];
    uint8_t spa[4];
    uint8_t tha[6];
    uint8_t tpa[4];
} __attribute__((packed));

struct ip_hdr {
    uint8_t ver_ihl;
    uint8_t tos;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_frag;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint8_t src[4];
    uint8_t dst[4];
} __attribute__((packed));

struct icmp_hdr {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence;
} __attribute__((packed));

static uint8_t tx_buffer[1600];

static uint16_t swap16(uint16_t v) {
    return (uint16_t)((v << 8) | (v >> 8));
}

static uint16_t net_checksum(const uint8_t *data, size_t len) {
    uint32_t sum = 0;
    while (len > 1) {
        sum += ((uint32_t)data[0] << 8) | data[1];
        data += 2;
        len -= 2;
    }
    if (len) {
        sum += ((uint32_t)data[0] << 8);
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return (uint16_t)(~sum & 0xFFFFu);
}

static int ip_equal(const uint8_t a[4], const uint8_t b[4]) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

static void net_handle_arp(const struct eth_hdr *eth, const struct arp_pkt *arp) {
    if (swap16(arp->oper) != ARP_OP_REQUEST) {
        return;
    }
    if (!ip_equal(arp->tpa, NET_IP)) {
        return;
    }

    struct {
        struct eth_hdr eth;
        struct arp_pkt arp;
    } reply;

    for (int i = 0; i < 6; ++i) {
        reply.eth.dst[i] = eth->src[i];
        reply.eth.src[i] = NET_MAC[i];
    }
    reply.eth.type = swap16(ETH_TYPE_ARP);

    reply.arp.htype = swap16(ARP_HTYPE_ETH);
    reply.arp.ptype = swap16(ARP_PTYPE_IP);
    reply.arp.hlen = 6;
    reply.arp.plen = 4;
    reply.arp.oper = swap16(ARP_OP_REPLY);
    for (int i = 0; i < 6; ++i) {
        reply.arp.sha[i] = NET_MAC[i];
        reply.arp.tha[i] = arp->sha[i];
    }
    for (int i = 0; i < 4; ++i) {
        reply.arp.spa[i] = NET_IP[i];
        reply.arp.tpa[i] = arp->spa[i];
    }

    if (virtio_net_send((uint8_t *)&reply, sizeof(reply)) == 0) {
        serial_write("[NET] ARP reply sent\n");
    } else {
        serial_write("[NET] ARP reply failed\n");
    }
}

static void net_handle_icmp(const struct eth_hdr *eth, const struct ip_hdr *ip, const uint8_t *payload, uint16_t payload_len) {
    if (!ip_equal(ip->dst, NET_IP)) {
        return;
    }
    if (payload_len < sizeof(struct icmp_hdr)) {
        return;
    }
    const struct icmp_hdr *icmp = (const struct icmp_hdr *)payload;
    if (icmp->type != 8) {
        return;
    }

    uint16_t icmp_total = payload_len;
    struct icmp_hdr *icmp_reply = (struct icmp_hdr *)(tx_buffer + sizeof(struct eth_hdr) + sizeof(struct ip_hdr));
    const uint8_t *icmp_src = payload + sizeof(struct icmp_hdr);
    uint16_t icmp_data_len = (uint16_t)(payload_len - sizeof(struct icmp_hdr));

    icmp_reply->type = 0;
    icmp_reply->code = 0;
    icmp_reply->identifier = icmp->identifier;
    icmp_reply->sequence = icmp->sequence;

    for (uint16_t i = 0; i < icmp_data_len; ++i) {
        ((uint8_t *)icmp_reply + sizeof(struct icmp_hdr))[i] = icmp_src[i];
    }
    icmp_reply->checksum = 0;
    icmp_reply->checksum = swap16(net_checksum((const uint8_t *)icmp_reply, icmp_total));

    struct ip_hdr *ip_reply = (struct ip_hdr *)(tx_buffer + sizeof(struct eth_hdr));
    ip_reply->ver_ihl = 0x45;
    ip_reply->tos = 0;
    uint16_t ip_total = (uint16_t)(sizeof(struct ip_hdr) + icmp_total);
    ip_reply->total_length = swap16(ip_total);
    ip_reply->identification = 0;
    ip_reply->flags_frag = 0;
    ip_reply->ttl = 64;
    ip_reply->protocol = IP_PROTO_ICMP;
    for (int i = 0; i < 4; ++i) {
        ip_reply->src[i] = NET_IP[i];
        ip_reply->dst[i] = ip->src[i];
    }
    ip_reply->checksum = 0;
    ip_reply->checksum = swap16(net_checksum((const uint8_t *)ip_reply, sizeof(struct ip_hdr)));

    struct eth_hdr *eth_reply = (struct eth_hdr *)tx_buffer;
    for (int i = 0; i < 6; ++i) {
        eth_reply->dst[i] = eth->src[i];
        eth_reply->src[i] = NET_MAC[i];
    }
    eth_reply->type = swap16(ETH_TYPE_IP);

    uint16_t total_len = (uint16_t)(sizeof(struct eth_hdr) + sizeof(struct ip_hdr) + icmp_total);
    if (virtio_net_send(tx_buffer, total_len) == 0) {
        serial_write("[NET] ICMP echo reply sent\n");
    } else {
        serial_write("[NET] ICMP echo reply failed\n");
    }
}

static void net_frame_handler(const uint8_t *frame, uint16_t len) {
    if (len < sizeof(struct eth_hdr)) {
        return;
    }
    const struct eth_hdr *eth = (const struct eth_hdr *)frame;
    uint16_t eth_type = swap16(eth->type);
    switch (eth_type) {
        case ETH_TYPE_ARP:
            if (len >= sizeof(struct eth_hdr) + sizeof(struct arp_pkt)) {
                const struct arp_pkt *arp = (const struct arp_pkt *)(frame + sizeof(struct eth_hdr));
                net_handle_arp(eth, arp);
            }
            break;
        case ETH_TYPE_IP:
            if (len >= sizeof(struct eth_hdr) + sizeof(struct ip_hdr)) {
                const struct ip_hdr *ip = (const struct ip_hdr *)(frame + sizeof(struct eth_hdr));
                uint16_t ip_header_len = (uint16_t)((ip->ver_ihl & 0x0Fu) * 4u);
                if (len < sizeof(struct eth_hdr) + ip_header_len) {
                    break;
                }
                uint16_t total_length = swap16(ip->total_length);
                if (total_length < ip_header_len || total_length > (len - sizeof(struct eth_hdr))) {
                    break;
                }
                if (ip->protocol == IP_PROTO_ICMP) {
                    const uint8_t *icmp_payload = frame + sizeof(struct eth_hdr) + ip_header_len;
                    uint16_t icmp_len = (uint16_t)(total_length - ip_header_len);
                    net_handle_icmp(eth, ip, icmp_payload, icmp_len);
                }
            }
            break;
        default:
            break;
    }
}

void net_init(void) {
    if (virtio_net_init(NET_MAC) != 0) {
        serial_write("[NET] virtio init failed\n");
    }
}

void net_poll(void) {
    virtio_net_poll(net_frame_handler);
}

void net_send_arp_probe(const uint8_t ip[4]) {
    struct {
        struct eth_hdr eth;
        struct arp_pkt arp;
    } req;

    for (int i = 0; i < 6; ++i) {
        req.eth.dst[i] = 0xFF;
        req.eth.src[i] = NET_MAC[i];
    }
    req.eth.type = swap16(ETH_TYPE_ARP);

    req.arp.htype = swap16(ARP_HTYPE_ETH);
    req.arp.ptype = swap16(ARP_PTYPE_IP);
    req.arp.hlen = 6;
    req.arp.plen = 4;
    req.arp.oper = swap16(ARP_OP_REQUEST);
    for (int i = 0; i < 6; ++i) {
        req.arp.sha[i] = NET_MAC[i];
        req.arp.tha[i] = 0;
    }
    for (int i = 0; i < 4; ++i) {
        req.arp.spa[i] = NET_IP[i];
        req.arp.tpa[i] = ip[i];
    }

    if (virtio_net_send((uint8_t *)&req, sizeof(req)) == 0) {
        serial_write("[NET] ARP probe sent\n");
    } else {
        serial_write("[NET] ARP probe failed\n");
    }
}
