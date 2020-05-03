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

int force_quit = 0;

void handle_client(int sockid) {
	int r, w;
	uint8_t buffer[MAX_BUFF];
	size_t rx, tx;
	rx = tx = 0;

	while (!force_quit) {
		r = SCTP_READ(sockid, buffer, MAX_BUFF);
		if (r == -1) {
			TRACE_ERROR("An error occured while reading from client\n");
			goto exit;
		}
		rx += r;

		TRACE_DEBUG("Received %d bytes from client\n", r);
		
		w = 0;
		while (w < r) {
			w += SCTP_WRITE(sockid, buffer + w, r - w);
			if (w < r) {
				TRACE_ERROR("Tried to send %d bytes but only sent %d btyes", r, w);
				if (w == -1) {
					TRACE_INFO("Closing client connection because sctp_sendmsg returned -1");
					goto exit;
				}
			}

			tx += w;
			if (force_quit) goto exit;
		}
	}
exit:
	close(sockid);
	TRACE_INFO("Received %ld bytes and sent %ld bytes\n", rx, tx);
}

void handle_sigint(int sig)  {
	printf("Caught signal %d, going to quit!\n", sig);
	force_quit = 1;
} 

int main() {
	int server_sock, client_sock;
	int ret;
	struct sockaddr_in servaddr;
	struct sctp_initmsg initmsg;

	signal(SIGINT, handle_sigint);

	server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
	if (server_sock == -1) {
		TRACE_ERROR("Failed to create server socket\n");
		goto socket_failed;
	}

	bzero((void *)&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORT);
	ret = bind(server_sock, (struct sockaddr *)&servaddr, sizeof(servaddr));
	if (ret == -1) {
		TRACE_ERROR("Failed to bind the server socket\n");
		goto failed_exit;
	}

	/* Specify that a maximum of 5 streams will be available per socket */
	memset(&initmsg, 0, sizeof(initmsg));
	initmsg.sinit_num_ostreams = 5;
	initmsg.sinit_max_instreams = 5;
	initmsg.sinit_max_attempts = 4;
	ret = setsockopt(server_sock, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg));
	if (ret == -1) {
		TRACE_ERROR("Unable to set socket options on server socket\n");
		goto failed_exit;
	}

	ret = listen(server_sock, 5);
	if (ret == -1) {
		TRACE_ERROR("Unable to listen on the server socket\n");
		goto failed_exit;
	}
	TRACE_INFO("Listening on the server socket!\n");

	TRACE_INFO("Awaiting a new connection\n");

	client_sock = accept(server_sock, NULL, NULL);
	if (client_sock == -1) {
		TRACE_ERROR("Could not accept new connection!\n");
		goto failed_exit;
	}

	TRACE_INFO("Accepted new client\n");
	handle_client(client_sock);

	close(server_sock);
	exit(EXIT_SUCCESS);

failed_exit:
	close(server_sock);
socket_failed:
	exit(EXIT_FAILURE);
}
