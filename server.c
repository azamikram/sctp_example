#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>

#include "debug.h"

#define MAX_BUFF 1024
#define PORT 8877

void handle_client(int sockid) {
	int rx, tx;
	int flags = 0;
	char buffer[MAX_BUFF];
	struct sctp_sndrcvinfo sndrcvinfo;

	while (1) {
		bzero(buffer, MAX_BUFF);

		rx = sctp_recvmsg(sockid, buffer, sizeof(buffer),
							NULL, 0, &sndrcvinfo, &flags);
		if (rx == -1) {
			TRACE_ERROR("An error occured while reading from client\n");
			continue;
		}

		TRACE_INFO("Received %d bytes from client, message: %s\n", rx, buffer);
		
		tx = sctp_sendmsg(sockid, buffer, rx, NULL, 0, 0, 0, 0, 0, 0);
		if (tx < rx) {
			TRACE_ERROR("Tried to send %d bytes but only sent %d btyes", rx, tx);
			if (tx == -1) {
				TRACE_INFO("Closing client connection because sctp_sendmsg returned -1");
				close(sockid);
				break;
			}
		}
	}
}

int main() {
	int server_sock, client_sock;
	int ret;
	struct sockaddr_in servaddr;
	struct sctp_initmsg initmsg;

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
		goto socket_error;
	}

	/* Specify that a maximum of 5 streams will be available per socket */
	memset(&initmsg, 0, sizeof(initmsg));
	initmsg.sinit_num_ostreams = 5;
	initmsg.sinit_max_instreams = 5;
	initmsg.sinit_max_attempts = 4;
	ret = setsockopt(server_sock, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg));
	if (ret == -1) {
		TRACE_ERROR("Unable to set socket options on server socket\n");
		goto socket_error;
	}

	ret = listen(server_sock, 5);
	if (ret == -1) {
		TRACE_ERROR("Unable to listen on the server socket\n");
		goto socket_error;
	}
	TRACE_INFO("Listening on the server socket!\n");

	while (1) {
		TRACE_INFO("Awaiting a new connection\n");

		client_sock = accept(server_sock, NULL, NULL);
		if (client_sock == -1) {
			TRACE_ERROR("Could not accept new connection!\n");
			continue;
		}
		else {
			TRACE_INFO("Accepted new client\n");
		}

		handle_client(client_sock);
	}

	exit(EXIT_SUCCESS);

socket_error:
	close(server_sock);
socket_failed:
	exit(EXIT_FAILURE);
}
