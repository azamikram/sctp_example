#include <stdlib.h>
#include <sys/time.h>

#include "common.h"

micro_ts_t micro_ts() {
	struct timeval now;
	micro_ts_t ts;

	gettimeofday(&now, NULL);
	ts = SEC_TO_MICRO(now.tv_sec);
	ts += now.tv_usec;
	return ts;
}