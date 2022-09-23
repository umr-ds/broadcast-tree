#define _GNU_SOURCE

#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/ip.h>
#include <errno.h>
#include <argp.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <netinet/ether.h>
#include <sys/socket.h>

struct arguments
{
    char *interface;
    uint16_t pps;
};

const char *argp_program_version = "flooding test 0.1";
const char *argp_program_bug_address = "<sterz@mathematik.uni-marburg.de>";
static char doc[] = "Test flooding of WiFi frames";
static char args_doc[] = "INTERFACE";
static struct argp_option options[] = {
    {"pps", 'p', "number", 0, "Packets per second", 1},

    {0}};

struct sockaddr_ll L_SOCKADDR = {
    .sll_family = 0,
    .sll_protocol = 0,
    .sll_ifindex = 0,
    .sll_hatype = 0,
    .sll_pkttype = 0,
    .sll_halen = 0,
    .sll_addr = {0}};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    struct arguments *arguments = state->input;

    char buf[PATH_MAX]; /* PATH_MAX incudes the \0 so +1 is not required */
    char *res;

    switch (key)
    {
    case 'p':
        arguments->pps = (uint16_t)strtol(arg, NULL, 10);
        break;
    case ARGP_KEY_ARG:
        if (state->arg_num >= 1)
            argp_usage(state);
        arguments->interface = arg;
        break;
    case ARGP_KEY_END:
        if (state->arg_num < 1)
            argp_usage(state);
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc, 0, 0, 0};

uint64_t get_time_msec()
{
    struct timeval tval;
    if (gettimeofday(&tval, NULL) != 0)
    {
        printf("Get time returned an error: %s\n", strerror(errno));
    }
    return ((tval.tv_sec * 1000000) + tval.tv_usec) / 1000;
}

int init(char *if_name)
{
    printf("Initialising socket. [interface: %s]\n", if_name);
    int ioctl_stat;
    int tmp_sockfd;

    struct ifreq if_idx;
    struct ifreq if_mac;

    if ((tmp_sockfd = socket(AF_PACKET, SOCK_RAW, htons(35039))) == -1)
    {
        printf("Could not create socket: %s\n", strerror(errno));
        return tmp_sockfd;
    }
    printf("Created socket. [sock_fd: %i]\n", tmp_sockfd);

    memcpy(if_idx.ifr_name, if_name, IFNAMSIZ - 1);
    ioctl_stat = ioctl(tmp_sockfd, SIOCGIFINDEX, &if_idx);
    if (ioctl_stat < 0)
    {
        printf("Could not get the interface's index. [%s]\n", strerror(errno));
    }
    printf("Got interface's index.\n");

    memcpy(if_mac.ifr_name, if_name, IFNAMSIZ - 1);
    ioctl_stat = ioctl(tmp_sockfd, SIOCGIFHWADDR, &if_mac);
    if (ioctl_stat < 0)
    {
        printf("Could not get MAC address. [%s]\n", strerror(errno));
        return ioctl_stat;
    }
    printf("Acquired MAC address.\n");

    L_SOCKADDR.sll_ifindex = if_idx.ifr_ifindex;
    L_SOCKADDR.sll_halen = ETH_ALEN;

    return tmp_sockfd;
}

void send_frame(int sockfd)
{
    uint8_t bcast_addr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    struct ether_header eth = {0x0};
    eth.ether_type = htons(35039);
    memcpy(eth.ether_dhost, bcast_addr, 6);
    memcpy(eth.ether_shost, L_SOCKADDR.sll_addr, 6);

    ssize_t sent_bytes = sendto(sockfd, &eth, sizeof(struct ether_header), 0, (struct sockaddr *)&L_SOCKADDR, sizeof(struct sockaddr_ll));

    if (sent_bytes < 0)
    {
        printf("Could not send data. [error: %s]\n", strerror(errno));
    }

    printf("Successfully sent frame. [sent_bytes: %i]\n", sent_bytes);
}

int event_loop(uint16_t pps, int sockfd)
{
    uint64_t wait_time = 1000 / pps;

    uint64_t last_frame_sent = get_time_msec();
    while (1)
    {
        uint64_t cur_time = get_time_msec();

        if (cur_time - last_frame_sent < wait_time)
        {
            uint64_t timeout = wait_time - (cur_time - last_frame_sent);
            printf("Have to wait. [timeout: %llu]\n", timeout);
            usleep(timeout * 1000);
        }

        send_frame(sockfd);
        last_frame_sent = get_time_msec();
    }
}

int main(int argc, char **argv)
{

    struct arguments arguments = {
        .interface = "",
        .pps = 100,
    };
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    int sockfd = init(arguments.interface);
    if (sockfd < 0)
    {
        exit(sockfd);
    }

    int res = event_loop(arguments.pps, sockfd);

    return res;
}
