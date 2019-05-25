/* -*- mode: C; indent-tabs-mode: t; tab-width: status[$1]; c-basic-offset: 4 -*- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#ifdef FREEBSD
#include <mysql.h>
#include <math.h>
#else
#include <mysql/mysql.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#define _XOPEN_SOURCE
#define __USE_XOPEN
#define _GNU_SOURCE
#include <time.h>

extern char HOST[];
extern char USER[];
extern char PASSWD[];
extern char DATABASE[];
extern char DATA_PREFIX[];
extern char SRC_PREFIX[];
extern int SERVER_ID;

extern const char status[][30];

#define STAT_WAITING status[0]
#define STAT_RUNNING status[1]
#define STAT_AC status[2]
#define STAT_CE status[3]
#define STAT_WA status[4]
#define STAT_RE status[5]
#define STAT_TLE status[6]
#define STAT_MLE status[7]
#define STAT_OLE status[8]
#define STAT_PE status[9]
#define STAT_RF status[10]
#define STAT_OUT_OF_CONTEST_TIME status[11]
#define STAT_OTHER status[12]
#define STAT_COMPILING status[13]
#define STAT_JUDGING status[14]
#define STAT_SF status[15]
#define STAT_FPE status[16]

struct lang_t
{
	char *language;
	char *compiler;
	char *ext;
	char *ccmd;
};

extern const struct lang_t lang[];

void parse_config(int argc, char **argv);
