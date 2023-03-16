#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "builtin.h"
#include "parse.h"

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

void builtin_execute (Task T)
{
    if (!strcmp (T.cmd, "exit")) {
        exit (EXIT_SUCCESS);
    } else if (!strcmp (T.cmd, "which")) {
		if (T.argv[1] == NULL) {
			fprintf(stderr, "error: the which command requires an argument.\n");
			exit (EXIT_SUCCESS);
		}	
		which(T.argv[1]);
	}
    else {
        printf ("pssh: builtin command: %s (not implemented!)\n", T.cmd);
    }
}
