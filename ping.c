#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip_icmp.h>
#include <time.h>
#include <signal.h>

#define PING_PKT_S 64
#define PORT_NO 0
#define PING_SLEEP_RATE 1000000
#define RECV_TIMEOUT 1

int pingloop = 1;

struct ping_pkt {
    struct icmphdr hdr;
    char msg[PING_PKT_S - sizeof(struct icmphdr)];
};

unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(unsigned char *)buf;
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

void intHandler(int dummy) { 
    pingloop = 0; 
}

char *dns_lookup(char *addr_host, struct sockaddr_in *addr_con) {
    struct hostent *host_entity = gethostbyname(addr_host);
    if (!host_entity) {
        return NULL;
    }

    char *ip = inet_ntoa(*(struct in_addr *)host_entity->h_addr);
    addr_con->sin_family = host_entity->h_addrtype;
    addr_con->sin_port = htons(PORT_NO);
    addr_con->sin_addr.s_addr = *(long *)host_entity->h_addr;

    return strdup(ip);
}

void send_ping(int ping_sockfd, struct sockaddr_in *ping_addr, char *ping_ip) {
    struct ping_pkt pckt;
    struct sockaddr_in r_addr;
    struct timespec time_start, time_end, tfs, tfe;
    struct timeval tv_out = {RECV_TIMEOUT, 0};
    int ttl_val = 64, msg_count = 0, addr_len, msg_received_count = 0;
    long double total_msec = 0;

    setsockopt(ping_sockfd, SOL_IP, IP_TTL, &ttl_val, sizeof(ttl_val));
    setsockopt(ping_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out));

    clock_gettime(CLOCK_MONOTONIC, &tfs);

    while (pingloop) {
        memset(&pckt, 0, sizeof(pckt));
        pckt.hdr.type = ICMP_ECHO;
        pckt.hdr.un.echo.id = getpid();
        for (int i = 0; i < sizeof(pckt.msg) - 1; i++) {
            pckt.msg[i] = i + '0';
        }
        pckt.hdr.un.echo.sequence = msg_count++;
        pckt.hdr.checksum = checksum(&pckt, sizeof(pckt));

        usleep(PING_SLEEP_RATE);

        clock_gettime(CLOCK_MONOTONIC, &time_start);
        if (sendto(ping_sockfd, &pckt, sizeof(pckt), 0, (struct sockaddr *)ping_addr, sizeof(*ping_addr)) <= 0) {
            continue;
        }

        addr_len = sizeof(r_addr);
        if (recvfrom(ping_sockfd, &pckt, sizeof(pckt), 0, (struct sockaddr *)&r_addr, &addr_len) > 0) {
            clock_gettime(CLOCK_MONOTONIC, &time_end);

            double time_elapsed = ((double)(time_end.tv_nsec - time_start.tv_nsec)) / 1000000.0;
            long double rtt_msec = (time_end.tv_sec - time_start.tv_sec) * 1000.0 + time_elapsed;

            printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%Lf ms\n", PING_PKT_S, ping_ip, msg_count, ttl_val, rtt_msec);
            msg_received_count++;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &tfe);
    double total_time_elapsed = ((double)(tfe.tv_nsec - tfs.tv_nsec)) / 1000000.0;
    total_msec = (tfe.tv_sec - tfs.tv_sec) * 1000.0 + total_time_elapsed;

    printf("\n--- %s ping ended ---\n", ping_ip);
    printf("%d packets transmitted, %d received, %.2f%% packet loss, time %Lf ms\n",
           msg_count, msg_received_count, ((msg_count - msg_received_count) / (double)msg_count) * 100.0, total_msec);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <hostname>\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct sockaddr_in addr_con;
    char *ip_addr = dns_lookup(argv[1], &addr_con);
    if (!ip_addr) {
        fprintf(stderr, "DNS lookup failed for %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("Socket creation failed");
        free(ip_addr);
        return EXIT_FAILURE;
    }

    signal(SIGINT, intHandler);
    send_ping(sockfd, &addr_con, ip_addr);

    close(sockfd);
    free(ip_addr);
    return EXIT_SUCCESS;
}

