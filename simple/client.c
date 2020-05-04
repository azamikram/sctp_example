#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <signal.h>

#include "debug.h"
#include "common.h"

#define DST_ADDR "127.0.0.1"
#define PORT 8877
#define MAX_BUFF 1024

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

void handle_connection(int sockid) {
	int r, w;
	uint8_t buffer[MAX_BUFF];
	uint8_t *data = generate_msg(MAX_BUFF / 2);
	size_t datalen = MAX_BUFF / 2;
#ifdef RATE
	micro_ts_t rx_start_ts, rx_end_ts;
	rx_start_ts = rx_end_ts = 0;

	micro_ts_t tx_start_ts, tx_end_ts;
	tx_start_ts = tx_end_ts = 0;

	size_t rx, tx;
	rx = tx = 0;
#endif

	while (!force_quit) {
		w = 0;
		// Send the complete message.
		while (w < datalen) {
#ifdef RATE
			if (tx == 0) tx_start_ts = micro_ts();
#endif
			w += SCTP_WRITE(sockid, data + w, datalen - w);
			if (w < datalen) {
				TRACE_INFO("Tried to read %ld bytes, read %d btyes\n", datalen, w);
				if (w == -1) {
					TRACE_ERROR("An error occured while reading from server\n");
					goto exit;
				}
			}
#ifdef RATE
			tx += w;
			tx_end_ts = micro_ts();
#endif
			if (force_quit) goto exit;
		}

		TRACE_DEBUG("Sent %d bytes and now trying to read %ld bytes\n", w, datalen);

#ifdef RATE
		if (rx == 0) rx_start_ts = micro_ts();
#endif
		r = SCTP_READ(sockid, buffer, datalen);
		if (r < datalen) {
			TRACE_INFO("Tried to read %ld bytes, read %d\n", datalen, r);
			if (r == -1) {
				TRACE_ERROR("An error occured while writing to server\n");
				goto exit;
			}
		}
#ifdef RATE
		rx += r;
		rx_end_ts = micro_ts();
#endif
	}
exit:
	close(sockid);
#ifdef RATE
	double rx_elapsed = MICRO_TO_SEC(rx_end_ts - rx_start_ts);
	double tx_elapsed = MICRO_TO_SEC(tx_end_ts - tx_start_ts);

	TRACE_INFO("The total time it took for rx: %0.4f sec\n", rx_elapsed);
	TRACE_INFO("The total time it took for rx: %0.4f sec\n", tx_elapsed);
	TRACE_INFO("Received %ld bytes and sent %ld bytes\n", rx, tx);
	TRACE_INFO("RX rate: %0.4fGbps | TX rate: %0.4fGbps\n",
				BYTES_TO_BITS(BYTES_TO_GB(rx)) / rx_elapsed,
				BYTES_TO_BITS(BYTES_TO_GB(tx)) / tx_elapsed);
#endif
}

void handle_sigint(int sig)  {
	printf("Caught signal %d, going to quit!\n", sig);
	force_quit = 1;
} 

int main(int argc, char *argv[]) {
	int sockid, ret;
	struct sockaddr_in servaddr;

	signal(SIGINT, handle_sigint); 

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
		TRACE_ERROR("Unable to connect to the server\n");
		goto failed_exit;
	}
	TRACE_INFO("Connected with the server, sockid: %d\n", sockid);

	handle_connection(sockid);
	close(sockid);
	exit(EXIT_SUCCESS);

failed_exit:
	close(sockid);
socket_failed:
	exit(EXIT_FAILURE);
}
