#ifndef COMMON_H_
#define COMMON_H_

#define SCTP_READ(sockid, msg, len)	 sctp_recvmsg(sockid, msg, len, NULL, 0, NULL, NULL)
#define SCTP_WRITE(sockid, msg, len) sctp_sendmsg(sockid, msg, len, NULL, 0, 0, 0, 0, 0, 0);

#endif /* COMMON_H_ */