#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>

#include "builtin.h"
#include "parse.h"
#include "jobs.h"

static char* builtin[] = {
    "exit",   /* exits the shell */
    "which",  /* displays full path to command */
    NULL
};


int is_builtin (char* cmd)
{
    int i;

    for (i=0; builtin[i]; i++) {
        if (!strcmp (cmd, builtin[i]))
            return 1;
    }

    return 0;
}
 
void which (char * arg) {
    char* dir;
    char* tmp;
    char* PATH;
    char* state;
    char probe[PATH_MAX];

    int found = 0;

    if (access (arg, X_OK) == 0) {
		fprintf(stdout, "%s\n", probe);
		exit(EXIT_SUCCESS);
	}
	
	if (is_builtin(arg)) {
		fprintf(stdout, "%s: shell build-in command\n", arg);
		exit(EXIT_SUCCESS);
	}

    PATH = strdup (getenv("PATH"));

    for (tmp=PATH; ; tmp=NULL) {
        dir = strtok_r (tmp, ":", &state);
        if (!dir)
            break;

        strncpy (probe, dir, PATH_MAX-1);
        strncat (probe, "/", PATH_MAX-1);
        strncat (probe, arg, PATH_MAX-1);

        if (access (probe, X_OK) == 0) {
            found = 1;
            break;
        }
    }
	if (found) {
		fprintf(stdout, "%s\n", probe);
	}
	free (PATH);
	exit(EXIT_SUCCESS);
}

int check_if_positive_integer(char *str) {
	while (*str) {
		if (!isdigit(*str)) { // Check if each character is a digit
			return 0;
		}
		str++;
	}
	return 1;
}

int convert_jobnum_to_int(char *job_num_arg) {
	if (job_num_arg[0] == '%' && check_if_positive_integer(job_num_arg + 1)) {
		return atoi(job_num_arg + 1);
	} 
	return -1;
}

pid_t convert_pid_to_int(char *pid_arg) {
	if (check_if_positive_integer(pid_arg)) {
		return atol(pid_arg);
	}
	return -1;
}

void builtin_execute (Task T)
{
	int job_int;
	pid_t pid_int;
    if (!strcmp (T.cmd, "exit")) {
        exit (EXIT_SUCCESS);
    } else if (!strcmp (T.cmd, "which")) {
		if (T.argv[1] == NULL) {
			fprintf(stderr, "error: the which command requires an argument.\n");
			exit (EXIT_SUCCESS);
		}	
		which(T.argv[1]);
	} else if (!strcmp (T.cmd, "jobs")) {
		JobArray_PrintJobs(&job_arr);
	} else if (!strcmp (T.cmd, "fg") || !strcmp(T.cmd, "bg")) {
		if (T.argv[1] == NULL) {
			fprintf(stderr, "error: the fg command requires an argument.\n");
			return;
		} else if ((job_int = convert_jobnum_to_int(T.argv[1])) != -1 &&
				job_arr.jobs[job_int] != NULL) {
			if (!strcmp(T.cmd, "fg")) {
				JobArray_MoveToFg(&job_arr, job_int);
			} else { // bg:
				JobArray_MoveToBg(&job_arr, job_int);
			}
		} else {
			fprintf(stderr, "pssh: invalid job number: %s.\n", T.argv[1]);
			return;
		}
	} else if (!strcmp (T.cmd, "kill")) {
		int sig = SIGTERM;
		int arg = 1;
		if (!strcmp(T.argv[1], "-s")) {
			sig = atoi(T.argv[2]);
			arg += 2;
		}
		for (; T.argv[arg] != NULL; arg++) {
			if (T.argv[arg][0] == '%') {
				if ((job_int = convert_jobnum_to_int(T.argv[arg])) != -1 &&
								job_arr.jobs[job_int] != NULL) {
					/* kill here */
					int e = killpg(job_arr.jobs[job_int]->pgid, sig);
					if (e != 0) {
						fprintf(stderr, "pssh: invalid job number: %s\n", T.argv[arg]);
					}
				} else {
					fprintf(stderr, "pssh: invalid job number: %s\n", T.argv[arg]);
				}
			} else {
				if ((pid_int = convert_jobnum_to_int(T.argv[arg]))) {
					/* kill here */
					int e = kill(pid_int, sig);
					if (e != 0) {
						fprintf(stderr, "pssh: invalid pid: %s\n", T.argv[arg]);
					}
				} else {
					fprintf(stderr, "pssh: invalid pid: %s\n", T.argv[arg]);
				}
			}
		}

	}
    else {
        printf ("pssh: builtin command: %s (not implemented!)\n", T.cmd);
    }
}
