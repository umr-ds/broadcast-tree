#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/ip.h>
#include <errno.h>
#include <poll.h>
#include <argp.h>
#include <time.h>
#include <limits.h>
#include <stdlib.h>
#include<signal.h>

#include "libexplain/socket.h"
#include "libexplain/ioctl.h"
#include <libexplain/poll.h>

#include "log.h"
#include "tree.h"
#include "btp.h"
#include "helpers.h"

extern self_t self;
extern bool payload_complete;
extern bool max_power;

uint16_t poll_timeout_msec;
uint16_t discovery_bcast_interval_msec;
uint16_t pending_timeout_msec;
uint16_t source_retransmit_payload_msec;

int init_sock(char *if_name, char *payload);
int event_loop(void);
void sig_handler(int signum);

struct arguments {
    char *payload;
    bool max_power;
    int log_level;
    char *log_file;
    char *interface;
    uint16_t poll_timeout_msec;
    uint16_t discovery_bcast_interval_msec;
    uint16_t pending_timeout_msec;
    uint16_t source_retransmit_payload_msec;
};

const char *argp_program_version = "btp 0.1";
const char *argp_program_bug_address = "<sterz@mathematik.uni-marburg.de>";
static char doc[] = "BTP -- Broadcast Tree Protocol";
static char args_doc[] = "INTERFACE";
static struct argp_option options[] = {
        {"source",    's', "payload", 0, "Path to the payload to be sent (omit this option for client mode)", 0 },
        {"max_power",  'm', 0,    0, "Whether to use maximum transmission power of fancy power calculations for efficiency", 0 },
        {"log_level", 'l', "level",   0, "Log level\n0: QUIET, 1: TRACE, 2: DEBUG, 3: INFO (default),\n4: WARN, 5: ERROR, 6: FATAL", 1 },
        {"log_file",  'f', "path",    0, "File path to log file.\nIf not present only stdout and stderr logging will be used", 1 },

        {"poll_timeout",  'p', "msec",    0, "Timeout for poll syscall", 1 },
        {"broadcast_timeout",  'b', "msec",    0, "How often the discovery frames should be broadcasted", 1 },
        {"pending_timeout",  't', "msec",    0, "How long to wait for potential parent to answer", 1 },
        {"retransmit_timeout",  'r', "msec",    0, "How long to wait for retransmitting the payload from the source", 1 },
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

void sig_handler(int signum){
    log_info("Received signal. Exiting. [signal: %i]", signum);
    exit(signum);
}

static error_t parse_opt (int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;

    char buf[PATH_MAX]; /* PATH_MAX incudes the \0 so +1 is not required */
    char *res;

    switch (key) {
        case 's':
            res = realpath(arg, buf);
            if (!res) {
                log_error("Could not read file. [arg= %s, err= %s]", arg, strerror(errno));
                exit(EXIT_FAILURE);
            }

            arguments->payload = res;
            break;
        case 'l':
            arguments->log_level = (int) strtol(arg, NULL, 10);
            break;
        case 'f':
            arguments->log_file = arg;
            break;
        case 'm':
            arguments->max_power = true;
            break;
        case 'p':
            arguments->poll_timeout_msec = (uint16_t) strtol(arg, NULL, 10);
            break;
        case 'b':
            arguments->discovery_bcast_interval_msec = (uint16_t) strtol(arg, NULL, 10);
            break;
        case 't':
            arguments->pending_timeout_msec = (uint16_t) strtol(arg, NULL, 10);
            break;
        case 'r':
            arguments->source_retransmit_payload_msec = (uint16_t) strtol(arg, NULL, 10);
            break;
        case ARGP_KEY_ARG : if (state->arg_num >= 1) argp_usage(state);
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

int init_sock(char *if_name, char *payload) {
    log_info("Initialising socket. [interface: %s]", if_name);
    int ioctl_stat;
    int tmp_sockfd;

    struct ifreq if_idx;
    struct ifreq if_mac;

    if ((tmp_sockfd = socket(AF_PACKET, SOCK_RAW, htons(BTP_ETHERTYPE))) == -1) {
        log_error("Could not create socket: %s", explain_socket(AF_PACKET, SOCK_RAW, htons(BTP_ETHERTYPE)));
        return tmp_sockfd;
    }
    log_debug("Created socket. [sock_fd: %i]", tmp_sockfd);

    memcpy(if_idx.ifr_name, if_name, IFNAMSIZ - 1);
    ioctl_stat = ioctl(tmp_sockfd, SIOCGIFINDEX, &if_idx);
    if (ioctl_stat < 0) {
        log_error("Could not get the interface's index. [%s]", explain_ioctl(tmp_sockfd, SIOCGIFINDEX, &if_idx));
    }
    log_debug("Got interface's index.");

    memcpy(if_mac.ifr_name, if_name, IFNAMSIZ - 1);
    ioctl_stat = ioctl(tmp_sockfd, SIOCGIFHWADDR, &if_mac);
    if (ioctl_stat < 0) {
        log_error("Could not get MAC address. [%s]", explain_ioctl(tmp_sockfd, SIOCGIFHWADDR, &if_mac));
        return ioctl_stat;
    }
    log_debug("Acquired MAC address. [addr: %s]", mac_to_str((uint8_t *)if_mac.ifr_hwaddr.sa_data));

    L_SOCKADDR.sll_ifindex = if_idx.ifr_ifindex;
    L_SOCKADDR.sll_halen = ETH_ALEN;

    init_self((uint8_t *)&if_mac.ifr_hwaddr.sa_data, payload, if_name, tmp_sockfd);
    log_debug("Initialized self. [source: %s, tree_id: %u]",  self.is_source ? "true" : "false", self.tree_id);

    int8_t max_tx_pwr;
    if ((max_tx_pwr = get_max_tx_pwr()) < 0) {
        return -1;
    }
    self.max_pwr = max_tx_pwr;
    log_debug("Figured out max sending power. [max_power: %i]", max_tx_pwr);

    return tmp_sockfd;
}

int event_loop(void) {
    ssize_t read_bytes;
    uint8_t recv_frame[MTU];
    memset(recv_frame, 0, MTU * sizeof (uint8_t));

    int bcast_send_time = get_time_msec();
    int res;
    log_info("Waiting for BTP packets.");
    while (1) {
        // If we have received the entire payload, we can shutdown or execution.
        // If if we received the entire payload and have no children, we disconnect from our parent to notify them,
        // that we are finished.
        if (payload_complete && hashmap_length(self.children) == 0) {
            log_info("Received entire payload and have no children. Disconnecting from parent.");
            disconnect_from_parent();
            return 0;
        }

        int cur_time = get_time_msec();
        if (cur_time - bcast_send_time > discovery_bcast_interval_msec) {
            if (self.is_source || self_is_connected()) {
                broadcast_discovery();
            }

            bcast_send_time = cur_time;
        }

        game_round(cur_time);
        log_debug("Evaluated game round.");

        struct pollfd pfd = {
            .fd = self.sockfd,
            .events = POLLIN
        };

        res = poll(&pfd, 1, poll_timeout_msec);

        if (res == -1) {
            log_error("Poll returned an error. [%s]", explain_poll(&pfd, 1, poll_timeout_msec));
            return res;
        }

        if (res == 0) {
            continue;
        }

        if (pfd.revents & POLLIN) {
            if ((read_bytes = recv(self.sockfd, recv_frame, MTU, 0)) >= 0) {
                log_info("Received BTP packet. [read_bytes: %i]", read_bytes);
                handle_packet(recv_frame);
            } else {
                if (errno == EINTR) {
                    memset(recv_frame, 0, MTU * sizeof (uint8_t));
                    log_error("Receive was interrupted. [error: %s]", strerror(errno));
                    continue;
                } else {
                    log_error("Receive returned and error. [error: %s]", strerror(errno));
                    return read_bytes;
                }
            }
        }
    }
}

int main (int argc, char **argv) {

    struct arguments arguments = {
            .payload = "",
            .max_power = false,
            .log_level = 3,
            .log_file = "",
            .interface = "",
            .poll_timeout_msec = 100,
            .discovery_bcast_interval_msec = 100,
            .pending_timeout_msec = 100,
            .source_retransmit_payload_msec = 100,
    };
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    max_power = arguments.max_power;
    poll_timeout_msec = arguments.poll_timeout_msec;
    discovery_bcast_interval_msec = arguments.discovery_bcast_interval_msec;
    pending_timeout_msec = arguments.pending_timeout_msec;
    source_retransmit_payload_msec = arguments.source_retransmit_payload_msec;

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

    signal(SIGINT, sig_handler);
    signal(SIGQUIT, sig_handler);
    signal(SIGKILL, sig_handler);
    signal(SIGTERM, sig_handler);

    int sockfd = init_sock(arguments.interface, arguments.payload);
    if (sockfd < 0){
        exit(sockfd);
    }

    if (strlen(arguments.payload) != 0) {
        broadcast_discovery();
    }

    int res = event_loop();

    if (res == 0) {
        log_info("Gracefully exiting.");
    }

    return res;
}
