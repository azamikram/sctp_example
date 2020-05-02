#ifndef DEBUG_H_
#define DEBUG_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define eprintf(args...) fprintf (stderr, args)

#ifdef ERROR
#define TRACE_ERROR(f, m...) { \
    eprintf("ERROR: [%s:%d] " f, __FILE__, __LINE__, ##m);   \
}
#else
#define TRACE_ERROR(f, m...)    (void)0
#endif /* ERROR */

#ifdef INFO
#define TRACE_INFO(f, m...) { \
    eprintf("INFO: [%s:%d] " f, __FILE__, __LINE__, ##m);   \
}
#else
#define TRACE_INFO(f, m...)    (void)0
#endif /* INFO */

#ifdef DEBUG
#define TRACE_DEBUG(f, m...) { \
    eprintf("DEBUG: [%s:%d] " f, __FILE__, __LINE__, ##m);   \
}
#else
#define TRACE_DEBUG(f, m...)    (void)0
#endif /* DEBUG */

#endif /* DEBUG_H_ */
