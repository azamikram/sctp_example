#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>

#include "debug.h"
#include "common.h"

#define DEAFULT_CLIENTS (5)
#define MAX_CPUS (100)

#define DST_ADDR "192.168.0.10"
#define PORT (8877)

#define MAX_BUFF (1024)

typedef struct client_stats {
	size_t rx;
	size_t tx;

	double rx_rate;
	double tx_rate;
} client_stats_t;

int force_quit = 0;

uint8_t* generate_msg(size_t len) {
	uint8_t byte = 0;
	uint8_t *msg = malloc(sizeof(uint8_t) * len);

	for (int i = 0; i < len; i++) {
		msg[i] = byte;
		byte = (byte + 1) % 256;
	}
	return msg;
}

int create_connection() {
	int sockid, ret, flags;
	struct sockaddr_in servaddr;

	sockid = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
	if (sockid == -1) {
		TRACE_ERROR("Failed to create server socket\n");
		goto socket_failed;
	}

	bzero((void *)&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(PORT);
	servaddr.sin_addr.s_addr = inet_addr(DST_ADDR);

	ret = connect(sockid, (struct sockaddr *)&servaddr, sizeof(servaddr));
	if (ret == -1) {
		TRACE_ERROR("Unable to connect to the server, error: %s\n", strerror(errno));
		goto failed_exit;
	}
	TRACE_INFO("Connected with the server, sockid: %d\n", sockid);

	flags = fcntl(sockid, F_GETFL, 0);
	ret = fcntl(sockid, F_SETFL, flags | O_NONBLOCK);
	if (ret == -1) {
		TRACE_ERROR("Unable to set socket as nonblocking, error: %s\n", strerror(errno));
		goto failed_exit;
	}

	return sockid;

failed_exit:
	close(sockid);
socket_failed:
	return FALSE;
}

void handle_connection(int sockid, client_stats_t *stats) {
	int r, w, ret;
	uint8_t buffer[MAX_BUFF];
	uint8_t *data = generate_msg(MAX_BUFF / 2);
	size_t datalen = MAX_BUFF / 2;
	size_t bytes_to_send;
#ifdef RATE
	micro_ts_t rx_start_ts, rx_end_ts;
	rx_start_ts = rx_end_ts = 0;

	micro_ts_t tx_start_ts, tx_end_ts;
	tx_start_ts = tx_end_ts = 0;
#endif

	while (!force_quit) {
		w = 0;
		bytes_to_send = datalen;
		// Send the complete message
		while (w < datalen) {
#ifdef RATE
			if (stats->tx == 0) tx_start_ts = micro_ts();
#endif
			ret = SCTP_WRITE(sockid, data + w, bytes_to_send);
			if (ret < 0 && errno != EAGAIN) {
				TRACE_ERROR("An error occur red while writing to server\n");
				goto exit;
			}
			if (force_quit) goto exit;
			if (ret == -1) continue;

			w += ret;
			bytes_to_send -= w;
#ifdef RATE
			stats->tx += w;
			tx_end_ts = micro_ts();
#endif
		}

		TRACE_DEBUG("Sent %d bytes and now trying to read %ld bytes\n", w, datalen);

#ifdef RATE
		if (stats->rx == 0) rx_start_ts = micro_ts();
#endif
		r = SCTP_READ(sockid, buffer, datalen);
		if (r <= 0) {
			if (ret == 0) {
				TRACE_ERROR("The connection closed from the server side, exiting\n");
				goto exit;
			} else if (errno != EAGAIN) {
				TRACE_ERROR("An error occured while reading from server\n");
				goto exit;
			}
		}
#ifdef RATE
		if (r == -1) continue;
		stats->rx += r;
		rx_end_ts = micro_ts();
#endif
	}
exit:
	close(sockid);
#ifdef RATE
	double rx_elapsed = MICRO_TO_SEC(rx_end_ts - rx_start_ts);
	double tx_elapsed = MICRO_TO_SEC(tx_end_ts - tx_start_ts);
	stats->rx_rate = BYTES_TO_BITS(BYTES_TO_GB(stats->rx)) / rx_elapsed;
	stats->tx_rate = BYTES_TO_BITS(BYTES_TO_GB(stats->tx)) / tx_elapsed;

#if 0
	TRACE_INFO("The total time it took for rx: %0.4f sec\n", rx_elapsed);
	TRACE_INFO("The total time it took for rx: %0.4f sec\n", tx_elapsed);
	TRACE_INFO("Received %ld bytes and sent %ld bytes\n", stats->rx, stats->tx);
	TRACE_INFO("RX rate: %0.4fGbps | TX rate: %0.4fGbps\n",
				stats->rx_rate, stats->tx_rate);
#endif
#endif
}

void* run_client(void *arg) {
	int sockid;
	client_stats_t *stats = (client_stats_t *)arg;
	stats->rx = stats->tx = 0;
	stats->rx_rate = stats->tx_rate = 0;

	sockid = create_connection();
	if (sockid == FALSE) return NULL;

	handle_connection(sockid, stats);
	close(sockid);

	return NULL;
}

void handle_sigint(int sig)  {
	printf("Caught signal %d, going to quit!\n", sig);
	force_quit = 1;
}

void usage(char *prog) {
  fprintf(stderr,
  				"usage: %s \n"
  				"	-n Number of clients, default is %d and maximum is %d\n"
				"	-h This help text\n",
				prog, DEAFULT_CLIENTS, MAX_CPUS);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
	int opt, n;
	pthread_t threads[MAX_CPUS];
	client_stats_t stats[MAX_CPUS];

	n = DEAFULT_CLIENTS;
	while ((opt = getopt(argc, argv, "n:h")) != -1) {
		switch(opt) {
			case 'n':
				n = atoi(optarg);
				if (n < 0 || n > MAX_CPUS) usage(argv[0]);
				break;
			case 'h':
			default:
				usage(argv[0]);
				break;
		}
	}

	signal(SIGINT, handle_sigint);

	for (int i = 0; i < n; i++) {
		pthread_create(threads + i, NULL, run_client, (void *)(stats + i));
	}

	for (int i = 0; i < n; i++) {
		pthread_join(threads[i], NULL);
	}

#ifdef RATE
	size_t rx, tx;
	double rx_rate, tx_rate;
	rx = tx = 0;
	rx_rate = tx_rate = 0;

	for (int i = 0; i < n; i++) {
		rx += stats[i].rx;
		tx += stats[i].tx;
		rx_rate += stats[i].rx_rate;
		tx_rate += stats[i].tx_rate;
	}
	TRACE_INFO("In summary:\n");
	TRACE_INFO("Received %ld bytes and sent %ld bytes\n", rx, tx);
	TRACE_INFO("RX rate: %0.4fGbps | TX rate: %0.4fGbps\n", rx_rate, tx_rate);
#endif

	exit(EXIT_SUCCESS);
}
