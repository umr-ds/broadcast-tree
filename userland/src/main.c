#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/ip.h>
#include <errno.h>
#include <poll.h>
#include <argp.h>

#include "btp.h"

struct arguments {
    bool source;
    char *interface;
};

const char *argp_program_version = "btp 0.1";
const char *argp_program_bug_address = "<sterz@mathematik.uni-marburg.de>";
static char doc[] = "BTP -- Broadcast Tree Protocol";
static char args_doc[] = "INTERFACE";
static struct argp_option options[] = {
        {"source",      's', 0,      0, "This node is a BTP source", 0 },
        { 0 }
};

struct sockaddr_ll L_SOCKADDR = {
        .sll_family = 0,
        .sll_protocol = 0,
        .sll_ifindex = 0,
        .sll_hatype = 0,
        .sll_pkttype = 0,
        .sll_halen = 0,
        .sll_addr = { 0 }
};

static error_t parse_opt (int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;

    switch (key) {
        case 's':
            arguments->source = true;
            break;
        case ARGP_KEY_ARG:
            if (state->arg_num >= 1) argp_usage (state);

            arguments->interface = arg;

            break;

        case ARGP_KEY_END:
            if (state->arg_num < 1) argp_usage (state);
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };

int sockfd = 0;
int init_sock(char *if_name, bool is_source) {
    int ioctl_stat;

    struct ifreq if_idx;
    struct ifreq if_mac;

    if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(BTP_ETHERTYPE))) == -1) {
        perror("Could not create socket");
        return sockfd;
    }

    memcpy(if_idx.ifr_name, if_name, IFNAMSIZ - 1);
    ioctl_stat = ioctl(sockfd, SIOCGIFINDEX, &if_idx);
    if (ioctl_stat < 0) {
        perror("Could not get the interface's index");
    }

    memcpy(if_mac.ifr_name, if_name, IFNAMSIZ - 1);
    ioctl_stat = ioctl(sockfd, SIOCGIFHWADDR, &if_mac);
    if (ioctl_stat < 0) {
        perror("Could not get MAC address");
        return ioctl_stat;
    }

    L_SOCKADDR.sll_ifindex = if_idx.ifr_ifindex;
    L_SOCKADDR.sll_halen = ETH_ALEN;

    init_self((uint8_t *)&if_mac.ifr_hwaddr.sa_data, 0, is_source);

    return sockfd;
}

int event_loop() {
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

int main (int argc, char **argv) {

    struct arguments arguments = {
            .source = false,
            .interface = ""
    };
    argp_parse (&argp, argc, argv, 0, 0, &arguments);


    sockfd = init_sock(arguments.interface, arguments.source);
    if (sockfd < 0){
        exit(sockfd);
    }

    if (arguments.source) {
        init_tree_construction();
    }

    return event_loop();
}
