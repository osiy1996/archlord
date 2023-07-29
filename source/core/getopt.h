#ifndef _GETOPT_H_
#define _GETOPT_H_

#include "core/macros.h"
#include "core/types.h"

BEGIN_DECLS

int getopt(int argc, char ** const argv, const char * ostr);

extern int optopt;     /* character checked for validity */
extern char * optarg;  /* argument associated with option */

END_DECLS

#endif /* _GETOPT_H_ */
