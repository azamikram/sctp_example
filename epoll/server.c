#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <signal.h>

#include "debug.h"
#include "common.h"

#define MAX_BUFF 1024
#define PORT 8877

int server_sock;
int force_quit = FALSE;
#ifdef RATE
micro_ts_t rx_start_ts, rx_end_ts;
micro_ts_t tx_start_ts, tx_end_ts;
size_t rx, tx;
#endif

int setup_listener() {
	int ret;
	struct sockaddr_in servaddr;
	struct sctp_initmsg initmsg;

	server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
	if (server_sock == -1) {
		TRACE_ERROR("Failed to create server socket\n");
		goto sock_failed;
	}

	bzero((void *)&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORT);
	ret = bind(server_sock, (struct sockaddr *)&servaddr, sizeof(servaddr));
	if (ret == -1) {
		TRACE_ERROR("Failed to bind the server socket\n");
		goto failed_return;
	}

	/* Specify that a maximum of 5 streams will be available per socket */
	memset(&initmsg, 0, sizeof(initmsg));
	initmsg.sinit_num_ostreams = 5;
	initmsg.sinit_max_instreams = 5;
	initmsg.sinit_max_attempts = 4;
	ret = setsockopt(server_sock, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg));
	if (ret == -1) {
		TRACE_ERROR("Unable to set socket options on server socket\n");
		goto failed_return;
	}

	ret = listen(server_sock, 5);
	if (ret == -1) {
		TRACE_ERROR("Unable to listen on the server socket\n");
		goto failed_return;
	}
	
	TRACE_INFO("Returing from handl_client\n");
	return TRUE;

failed_return:
	close(server_sock);
sock_failed:
	return FALSE;
}

int accept_conn() {
	int ret;

	TRACE_INFO("Awaiting a new connection\n");
	ret = accept(server_sock, NULL, NULL);
	if (ret == -1) {
		TRACE_ERROR("Could not accept new connection!\n");
		return FALSE;
	}
	return ret;
}

int handle_write(int sockid, uint8_t *buffer, size_t len) {
	int w = 0;
	while (w < len) {
#ifdef RATE
		if (tx == 0) tx_start_ts = micro_ts();
#endif
		w += SCTP_WRITE(sockid, buffer + w, len - w);
		if (w < len) {
			TRACE_ERROR("Tried to send %ld bytes but only sent %d btyes", len, w);
			if (w == -1) {
				TRACE_INFO("Closing client connection because sctp_sendmsg returned -1");
				return FALSE;
			}
		}
#ifdef RATE
		tx += w;
		tx_end_ts = micro_ts();
#endif
		if (force_quit) return FALSE;
	}
	return TRUE;
}

int handle_read(int sockid) {
	int ret;
	uint8_t buffer[MAX_BUFF];

#ifdef RATE
	if (rx == 0) rx_start_ts = micro_ts();
#endif
	ret = SCTP_READ(sockid, buffer, MAX_BUFF);
	if (ret == -1) {
		TRACE_ERROR("An error occured while reading from client\n");
		return FALSE;
	}
#ifdef RATE
	if (ret != 0) {
		rx += ret;
		rx_end_ts = micro_ts();
	}
#endif

	TRACE_DEBUG("Received %d bytes from client\n", ret);
	return handle_write(sockid, buffer, ret);
}

void handle_client(int sockid) {
	while (!force_quit) {
		if (handle_read(sockid) == FALSE) break;
	}

	close(sockid);
}

void handle_sigint(int sig)  {
	printf("Caught signal %d, going to quit!\n", sig);
	force_quit = TRUE;
} 

int main() {
	int ret;
	signal(SIGINT, handle_sigint);

	TRACE_INFO("TRUE: %d | FALSE: %d | FALSE: %d\n", TRUE, FALSE, FALSE);

#ifdef RATE
	rx_start_ts = rx_end_ts = 0;
	tx_start_ts = tx_end_ts = 0;
	rx = tx = 0;
#endif

	ret = setup_listener();
	TRACE_INFO("Ret listener %d | FALSE: %d\n", ret, FALSE);
	if (ret == FALSE) goto listener_failed;
	TRACE_INFO("Listening on the server socket!\n");

	ret = accept_conn();
	if (ret == FALSE) goto failed_exit;
	TRACE_INFO("Accepted new client\n");

	handle_client(ret);

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
	close(server_sock);
	exit(EXIT_SUCCESS);

failed_exit:
	close(server_sock);
listener_failed:
	exit(EXIT_FAILURE);
}
