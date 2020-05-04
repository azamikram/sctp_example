#include <sys/epoll.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <signal.h>

#include "debug.h"
#include "common.h"

#define EPOLL_SIZE (1024)
#define BURST_SIZE (32)
#define MAX_BUFF (1024)
#define PORT (8877)

int server_sock = -1;
int epoll_fd = -1;
int force_quit = FALSE;
#ifdef RATE
micro_ts_t rx_start_ts, rx_end_ts;
micro_ts_t tx_start_ts, tx_end_ts;
size_t rx, tx;
#endif

int add_to_epoll(int events, int fd) {
  int ret;
  struct epoll_event ev;

  ev.events = events;
  ev.data.fd= fd;

  TRACE_INFO("Adding fd %d to epoll\n", fd);
  ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
  if (ret == -1) {
    TRACE_ERROR("Unable to add fd to epoll, epoll_ctl: %s\n", strerror(errno));
	return FALSE;
  }
  return TRUE;
}

int rm_from_epoll(int fd) {
  int ret;
  struct epoll_event ev;

  ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &ev);
  if (ret == -1) {
    TRACE_ERROR("Unable to remove fd from epoll, epoll_ctl: %s\n", strerror(errno));
	return FALSE;
  }
  return TRUE;
}

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
	TRACE_INFO("Returing from setup_listener\n");
	return TRUE;

failed_return:
	close(server_sock);
sock_failed:
	return FALSE;
}

int accept_conn() {
	int ret;

	TRACE_DEBUG("Waiting to accept a new client\n");
	ret = accept(server_sock, NULL, NULL);
	if (ret == -1) {
		TRACE_ERROR("Could not accept new connection!\n");
		goto return_failed;
	}
	TRACE_DEBUG("Accepted a new client\n");

	if (add_to_epoll(EPOLLIN, ret) == FALSE) goto epoll_failed;

	TRACE_INFO("Accepted new client\n");
	return ret;

epoll_failed:
	close(ret);
return_failed:
	return FALSE;
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

int read_event(int sockid) {
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
	if (ret == 0) {
		TRACE_INFO("Connection closed from the other side, exiting\n");
		return FALSE;
	}

	rx += ret;
	rx_end_ts = micro_ts();
#endif

	TRACE_DEBUG("Received %d bytes from client\n", ret);
	return handle_write(sockid, buffer, ret);
}

void handle_sigint(int sig)  {
	printf("Caught signal %d, going to quit!\n", sig);
	force_quit = TRUE;
} 

int main() {
	int ret, fd, nb_ev;
  	struct epoll_event ev[BURST_SIZE];

	signal(SIGINT, handle_sigint);

#ifdef RATE
	rx_start_ts = rx_end_ts = 0;
	tx_start_ts = tx_end_ts = 0;
	rx = tx = 0;
#endif

	ret = setup_listener();
	if (ret == FALSE) goto listener_failed;
	TRACE_INFO("Listening on the server socket!\n");

  	epoll_fd = epoll_create(EPOLL_SIZE); 
	if (epoll_fd == -1) {
		TRACE_ERROR("Unable to create epoll, epoll: %s\n", strerror(errno));
		goto failed_exit;
	}
	add_to_epoll(EPOLLIN, server_sock);

  	while (!force_quit) {
		TRACE_DEBUG("Wating for futher events...\n");
		nb_ev = epoll_wait(epoll_fd, ev, BURST_SIZE, -1);
		TRACE_DEBUG("Got %d events from epoll_wait\n", nb_ev);
		
		for (int i = 0; i < nb_ev; i++) {
			fd = ev[i].data.fd;
			TRACE_DEBUG("Processign %d event, Got an event againt fd: %d\n", i, fd);
			if (fd == server_sock)  {
				accept_conn();
				continue;
			}
			
			if (ev[i].events & EPOLLIN) {
				if (read_event(fd) == FALSE) {
					rm_from_epoll(fd);
					close(fd);
				}
			}
		}
	}

	close(server_sock);
	close(epoll_fd);

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
	exit(EXIT_SUCCESS);

failed_exit:
	close(server_sock);
listener_failed:
	exit(EXIT_FAILURE);
}
