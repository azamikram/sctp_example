#ifndef COMMON_H_
#define COMMON_H_

#define SEC_TO_MICRO(sec) ((sec) * 1e6)
#define MICRO_TO_SEC(micro) ((micro) * 1e-6)

#define BYTES_TO_BITS(bytes) ((bytes) * 8)
#define BYTES_TO_GB(bytes) ((bytes) * 1e-9)

#define SCTP_READ(sockid, msg, len)	 sctp_recvmsg(sockid, msg, len, NULL, 0, NULL, NULL)
#define SCTP_WRITE(sockid, msg, len) sctp_sendmsg(sockid, msg, len, NULL, 0, 0, 0, 0, 0, 0);

typedef double micro_ts_t;

// Returns current timestamp in microseconds
micro_ts_t micro_ts();

#endif /* COMMON_H_ */