#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/ip.h>
#include <errno.h>
#include <poll.h>

#include "btp.h"

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
int sockfd = 0;

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

int event_loop(int sockfd) {
    ssize_t read_bytes;
    uint8_t recv_frame[MTU];
    memset(recv_frame, 0, MTU * sizeof (uint8_t));

    int res;
    printf("Waiting for BTP Response.\n");
    while (1) {
        struct pollfd pfd = {
            .fd = sockfd,
            .events = POLLIN
        };

        res = poll(&pfd, 1, POLL_TIMEOUT);

        if (res == -1) {
            perror("Poll returned an error.");
            return res;
        }

        if (res == 0) {
            continue;
        }

        if (pfd.revents & POLLIN) {
            if ((read_bytes = recv(sockfd, recv_frame, MTU, 0)) >= 0) {
                printf("Received BTP response.\n");
                handle_packet(recv_frame);
                // hexdump(recv_frame, read_bytes);
            } else {
                if (errno == EINTR) {
                    memset(recv_frame, 0, MTU * sizeof (uint8_t));
                    perror("Received signal from recv.");
                    continue;
                } else {
                    perror("Could not receive data.");
                    return read_bytes;
                }
            }
        }
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

    init_tree_construction(sockfd, LADDR, L_SOCKADDR);

    return event_loop(sockfd);
}