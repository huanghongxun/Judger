/* -*- mode: C; indent-tabs-mode: t; tab-width: 4; c-basic-offset: 4 -*- */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#define ARG_LEN 1024
char HOST[ARG_LEN] = "";
char USER[ARG_LEN] = "";
char PASSWD[ARG_LEN] =	"";
char DATABASE[ARG_LEN] = "";
char DATA_PREFIX[ARG_LEN] = "/testdata";
char SRC_PREFIX[ARG_LEN] = "/source";
int SERVER_ID = 0;

const char status[][30] = {
	"Waiting",				// 0
	"Running",				// 1
	"Accepted",				// 2
	"Compile Error",		// 3
	"Wrong Answer",			// 4
	"Runtime Error",		// 5
	"Time Limit Exceeded",	// 6
	"Memory Limit Exceeded",// 7
	"Output Limit Exceeded",// 8
	"Presentation Error",	// 9
	"Restrict Function",	// 10
	"Out of Contest Time",	// 11
	"Other",	            // 12
	"Compiling",            // 13
	"Judging",              // 14
	"Segmentation Fault",   // 15
	"Floating Point Error"  // 16
};

static const char HELP_MSG[] =
	"Usage: judge [options]\n"
	"\n"
	"Available options:\n"
	"-H --host=sqlhost           Set the hostname of SQL server\n"
	"-u --user=sqluser           Set the username of SQL server\n"
	"-p --password=sqlpasswd     Set the password of the user in SQL server\n"
	"-d --database=sqldbname     Set the database to be used. Default is sicily\n"
	"-D --data-dir=dir           Set the path of test data\n"
	"-s --source-dir=dir         Set the path of submitted source codes\n"
	"-j --judge-id=jid           Set the ID of this judge\n"
	"-c --config=filepath        Load configuation from file\n"
	"-h --help                   Display this message\n"
	;
	
static struct option lopts[] = {
	{"host", required_argument, 0, 'H'},
	{"user", required_argument, 0, 'u'},
	{"password", required_argument, 0, 'p'},
	{"database", required_argument, 0, 'd'},
	{"data-dir", required_argument, 0, 'D'},
	{"source-dir", required_argument, 0, 's'},
	{"judge-id", required_argument, 0, 'j'},
	{"config", required_argument, 0, 'c'},
	{"help", no_argument, 0, 'h'},
};

const struct lang_t lang[] = {
	{"C", "gcc", "c", "gcc-8 -fno-asm -o %s %s -O2 -ansi -Wall -Wextra -lm --static > %s.log 2>&1"},
	{"C++", "g++", "cpp", "g++-8 -fno-asm -std=gnu++17 -o %s %s -O2 -Wall -Wextra -lm --static > %s.log 2>&1"},
	{"Pascal", "fpc", "pas", "fpc -o%s %s -O2 > %s.log 2>&1"},
	{"Java", "java", "java", "javac -d %s %s > %s.log 2>&1"}, 
	{NULL, NULL, NULL, NULL}
};

static const struct conf_strings_t {
	char *argname;
	char *val;
} conf_strings[] = {
	{"host", HOST},
	{"user", USER},
	{"password", PASSWD},
	{"database", DATABASE},
	{"data_dir", DATA_PREFIX},
	{"source_dir", SRC_PREFIX},
	{NULL, NULL},
};

static const char *sopts = "H:u:p:d:D:s:j:c:eh";

static void parse_file(const char *filepath) {
	FILE *file = fopen(filepath, "r");
	char buf[ARG_LEN];
	if (file == NULL) {
		fprintf (stderr, "Cannot open configure file: %s\n", filepath);
	}
	while (fgets(buf, ARG_LEN, file) != NULL) {
		char *argname = strtok(buf, "= \t\r\n");
		if (argname == NULL) continue;
		if (argname[0] == '#') continue;
		char *val = strtok(NULL, "= \t\r\n");
		if (val == NULL) continue;
		int i = 0;
		for (; conf_strings[i].argname; i++) {
			if (strcmp(argname, conf_strings[i].argname) == 0)
				strncpy(conf_strings[i].val, val, ARG_LEN);
		}
		if (strcmp(argname, "judge-id") == 0) {
			SERVER_ID = strtol(optarg, (char**)NULL, 10);
		}
	}
	fclose(file);
}

static void parse_env() {
    for (int i = 0; conf_strings[i].argname; i++) {
        const char *s = getenv(conf_strings[i].argname);
        if (s) {
            strncpy(conf_strings[i].val, s, ARG_LEN);
        }
    }
}

void parse_config(int argc, char **argv) {
	int opt, index;
	while ((opt = getopt_long(argc, argv, sopts, lopts, &index)) != -1) {
		switch (opt) {
		case 'H':
			strncpy(HOST, optarg, ARG_LEN);
			break;
		case 'u':
			strncpy(USER, optarg, ARG_LEN);
			break;
		case 'p':
			strncpy(PASSWD, optarg, ARG_LEN);
			break;
		case 'd':
			strncpy(DATABASE, optarg, ARG_LEN);
			break;
		case 'D':
			strncpy(DATA_PREFIX, optarg, ARG_LEN);
			break;
		case 's':
			strncpy(SRC_PREFIX, optarg, ARG_LEN);
			break;
		case 'j':
			errno = 0;
			SERVER_ID = strtol(optarg, (char**)NULL, 10);
			break;
		case 'c':
			parse_file(optarg);
			break;
        case 'e':
            parse_env();
            break;
		case 'h':
			printf("%s\n", HELP_MSG);
			exit(0);
			break;
		default:
			fprintf (stderr, "Invalid option: %s\n", argv[index]);
			break;
		}
	}
	if (strlen(USER) == 0) {
		fprintf( stderr, "Username must be provided\n" );
		exit(1);
	}
	if (strlen(PASSWD) == 0) {
		fprintf( stderr, "Password must be provided\n" );
		exit(1);
	}	
}
