#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ether.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>

#include "btp.h"
#include "helpers.h"

const char* IF_NAME = "enp34s0";
mac_addr_t LADDR = { 0x0 };
struct sockaddr_ll L_SOCKADDR = {
    .sll_family = 0,
    .sll_protocol = 0,
    .sll_ifindex = 0,
    .sll_hatype = 0,
    .sll_pkttype = 0,
    .sll_halen = 0,
    .sll_addr = { 0 }
};

int init_sock() {
	int sockfd;
    int ioctl_stat;

	struct ifreq if_idx;
	struct ifreq if_mac;

	if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(BTP_ETHERTYPE))) == -1) {
	    perror("Could not create socket");
        return sockfd;
	}

    memcpy(if_idx.ifr_name, IF_NAME, IFNAMSIZ - 1);
    ioctl_stat = ioctl(sockfd, SIOCGIFINDEX, &if_idx);
	if (ioctl_stat < 0) {
        perror("Could not get the interface's index");
        return ioctl_stat;
    }
  
    memcpy(if_mac.ifr_name, IF_NAME, IFNAMSIZ - 1);
    ioctl_stat = ioctl(sockfd, SIOCGIFHWADDR, &if_mac);
	if (ioctl_stat < 0) {
        perror("Could not get MAC address");
        return ioctl_stat;
    }

    L_SOCKADDR.sll_ifindex = if_idx.ifr_ifindex;
    L_SOCKADDR.sll_halen = ETH_ALEN;

    memcpy(LADDR, &if_mac.ifr_hwaddr.sa_data, 6);

    return sockfd;
}

void init_tree_construction(int sockfd) {
    btp_frame_t discovery_frame = {
        .eth = {
            .ether_dhost = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
            .ether_shost = { 0 },
            .ether_type = htons(BTP_ETHERTYPE)
        },
        .header = {
            .recv_err = 0,
            .game_fin = 0,
            .mutex = 0,
            .unused = 0,
            .frame_type = discovery,
            .tree_id = 0,
            .seq_num = 0,
            .tx_pwr = INT_MAX,
            .parent_addr = { 0 },
            .high_pwr = 0,
            .snd_high_pwr = 0
        }
    };

    unsigned char buf[10];
    time_t cur_time = time(NULL);
    memcpy(buf, LADDR, 6);
    memcpy(buf + 6, &cur_time, 4);
    discovery_frame.header.tree_id = gen_tree_id(buf);

    memcpy(discovery_frame.eth.ether_shost, LADDR, 6);

	/* Send packet */
	if (sendto(sockfd, &discovery_frame, sizeof(btp_frame_t), 0, (struct sockaddr*)&L_SOCKADDR, sizeof(struct sockaddr_ll)) < 0) {
	    perror("Could not send data");
        exit(1);
    }
}

int main () {

    /*
    if argv[1] == "source":
        construct_tree
    else:
        LISTEN
    */

	int sockfd;

    sockfd = init_sock();
    if (sockfd < 0){
        exit(sockfd);
    }

    init_tree_construction(sockfd);




	return 0;
}