#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/ip.h>
#include <errno.h>
#include <poll.h>
#include <argp.h>

#include "log.h"
#include "tree.h"
#include "btp.h"
#include "helpers.h"

extern self_t self;

struct arguments {
    bool source;
    int log_level;
    char *log_file;
    char *interface;
};

const char *argp_program_version = "btp 0.1";
const char *argp_program_bug_address = "<sterz@mathematik.uni-marburg.de>";
static char doc[] = "BTP -- Broadcast Tree Protocol";
static char args_doc[] = "INTERFACE";
static struct argp_option options[] = {
        {"source",      's', 0,      0, "This node is a BTP source", 0 },
        {"log",      'l', "level",      OPTION_ARG_OPTIONAL, "Log level\n0: QUIET, 1: TRACE, 2: DEBUG, 3: INFO (default),\n4: WARN, 5: ERROR, 6: FATAL", 1 },
        {"file",      'f', "path",      OPTION_ARG_OPTIONAL, "File path to log file.\nIf not present only stdout and stderr logging will be used.", 1 },
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
        case 'l':
            arguments->log_level = (int) strtol(arg, NULL, 10);
            break;
        case 'f':
            arguments->log_file = arg;
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

int init_sock(char *if_name, bool is_source) {
    int ioctl_stat;
    int tmp_sockfd;

    struct ifreq if_idx;
    struct ifreq if_mac;

    if ((tmp_sockfd = socket(AF_PACKET, SOCK_RAW, htons(BTP_ETHERTYPE))) == -1) {
        log_error("Could not create socket: %s", strerror(errno));
        return tmp_sockfd;
    }

    memcpy(if_idx.ifr_name, if_name, IFNAMSIZ - 1);
    ioctl_stat = ioctl(tmp_sockfd, SIOCGIFINDEX, &if_idx);
    if (ioctl_stat < 0) {
        log_error("Could not get the interface's index: %s", strerror(errno));
    }

    memcpy(if_mac.ifr_name, if_name, IFNAMSIZ - 1);
    ioctl_stat = ioctl(tmp_sockfd, SIOCGIFHWADDR, &if_mac);
    if (ioctl_stat < 0) {
        log_error("Could not get MAC address: %s", strerror(errno));
        return ioctl_stat;
    }

    L_SOCKADDR.sll_ifindex = if_idx.ifr_ifindex;
    L_SOCKADDR.sll_halen = ETH_ALEN;

    init_self((uint8_t *)&if_mac.ifr_hwaddr.sa_data, 0, is_source, if_name, tmp_sockfd);

    int8_t max_tx_pwr = get_max_tx_pwr();
    self.max_pwr = max_tx_pwr;

    return tmp_sockfd;
}

int event_loop() {
    ssize_t read_bytes;
    uint8_t recv_frame[MTU];
    memset(recv_frame, 0, MTU * sizeof (uint8_t));

    int res;
    log_info("Waiting for BTP packets.");
    while (1) {
        struct pollfd pfd = {
            .fd = self.sockfd,
            .events = POLLIN
        };

        res = poll(&pfd, 1, POLL_TIMEOUT);

        if (res == -1) {
            log_error("Poll returned an error: %s", strerror(errno));
            return res;
        }

        if (res == 0) {
            continue;
        }

        if (pfd.revents & POLLIN) {
            if ((read_bytes = recv(self.sockfd, recv_frame, MTU, 0)) >= 0) {
                log_info("Received BTP packet.");
                handle_packet(recv_frame);
            } else {
                if (errno == EINTR) {
                    memset(recv_frame, 0, MTU * sizeof (uint8_t));
                    log_error("Received signal from recv: %s", strerror(errno));
                    continue;
                } else {
                    log_error("Could not receive data: %s", strerror(errno));
                    return read_bytes;
                }
            }
        }
    }
}

int main (int argc, char **argv) {

    struct arguments arguments = {
            .source = false,
            .log_level = 3,
            .log_file = "",
            .interface = ""
    };
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    // Logging stuff
    if (arguments.log_level == 0) {
        log_set_quiet(true);
    } else {
        log_set_level(arguments.log_level - 1);
    }

    if (arguments.log_file[0] != '\0') {
        FILE *lf = fopen(arguments.log_file, "a");
        log_add_fp(lf, arguments.log_level - 1);
    }

    int sockfd = init_sock(arguments.interface, arguments.source);
    if (sockfd < 0){
        exit(sockfd);
    }

    if (arguments.source) {
        init_tree_construction();
    }

    return event_loop();
}
