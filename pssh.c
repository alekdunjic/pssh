#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <wait.h>

#include "builtin.h"
#include "parse.h"
#include "jobs.h"

#define READ_SIDE 0
#define WRITE_SIDE 1

/*******************************************
 * Set to 1 to view the command line parse *
 *******************************************/
#define DEBUG_PARSE 0

// TODO: Replace this with an actual name
char job_name[] = "job name";
int our_tty;
JobArray job_arr;

void set_fg_pgrp(pid_t pgrp)
{
    void (*sav)(int sig);

    if (pgrp == 0)
        pgrp = getpgrp();

    sav = signal(SIGTTOU, SIG_IGN);
    tcsetpgrp(our_tty, pgrp);
    signal(SIGTTOU, sav);
}

void handler(int sig) {
	pid_t chld;
	int status;
	switch(sig) {
		case SIGTTOU:
		case SIGTTIN:
			while(tcgetpgrp(STDOUT_FILENO) != getpgrp())
				pause();
			break;

		case SIGCHLD:
			while( (chld = waitpid(-1, &status, WNOHANG | WCONTINUED | WUNTRACED)) > 0 ) {
				if (WIFSTOPPED(status)) {
//					set_fg_pgrp(0);
//					printf("Parent: Child %d stopped! Continuing it!\n", chld);
//					set_fg_pgrp(getpgid(chld));
//					kill(chld, SIGCONT);
				} else if (WIFCONTINUED(status)) {
				} else {
					int j = JobArray_HandleChild(&job_arr, chld);
					set_fg_pgrp(0);
					printf("Child %d received\n", chld);
					set_fg_pgrp(getpgid(chld));
					if (j > -1) {
						set_fg_pgrp(0);
						printf("Parent: Process Group %d has terminated\n", job_arr.jobs[j]->pgid);
						JobArray_RemoveJob(&job_arr, j);
					}
				}
			}
			break;

		default:
			break;
	}
}

void print_banner ()
{
    printf ("                    ________   \n");
    printf ("_________________________  /_  \n");
    printf ("___  __ \\_  ___/_  ___/_  __ \\ \n");
    printf ("__  /_/ /(__  )_(__  )_  / / / \n");
    printf ("_  .___//____/ /____/ /_/ /_/  \n");
    printf ("/_/ Type 'exit' or ctrl+c to quit\n\n");
}

/* a dynamic string structure */
typedef struct dynamic_string {
	unsigned int size; // amount of memory allocated
	char *data; // the string itself 
} string;

/* initialize an empty string with a specified size */
string construct_string(unsigned int size) {
	char * data = malloc(size * sizeof(char));
	data[0] = '\0';
	string str = {size, data};
	return str;
}

/* resize the string to a specified new_size */
void resize_string(string *str, unsigned int new_size) {
	char *new_data = malloc(new_size*sizeof(char));	
	strcpy(new_data, str->data);
	free(str->data);
	str->data = new_data;
	str->size = new_size;
}

/* append chars to the string */
void add_chars(string *str, char * chars) {
	unsigned int required_size = strlen(str->data) + strlen(chars) + 1;
	if (required_size > str->size) {
		resize_string(str, required_size);
	}
	strcat(str->data, chars);
}

/* returns a string for building the prompt
 *
 * Note:
 *   If you modify this function to return a string on the heap,
 *   be sure to free() it later when appropirate!  */
static char* build_prompt ()
{
	string prompt = construct_string(10);
	while (getcwd(prompt.data, prompt.size) == NULL) {
		resize_string(&prompt, 2*prompt.size);
	}
	add_chars(&prompt, "$ ");
    return prompt.data;
}


/* return true if command is found, either:
 *   - a valid fully qualified path was supplied to an existing file
 *   - the executable file was found in the system's PATH
 * false is returned otherwise */
static int command_found (const char* cmd)
{
    char* dir;
    char* tmp;
    char* PATH;
    char* state;
    char probe[PATH_MAX];

    int ret = 0;

    if (access (cmd, X_OK) == 0)
        return 1;

    PATH = strdup (getenv("PATH"));

    for (tmp=PATH; ; tmp=NULL) {
        dir = strtok_r (tmp, ":", &state);
        if (!dir)
            break;

        strncpy (probe, dir, PATH_MAX-1);
        strncat (probe, "/", PATH_MAX-1);
        strncat (probe, cmd, PATH_MAX-1);

        if (access (probe, X_OK) == 0) {
            ret = 1;
            break;
        }
    }

    free (PATH);
    return ret;
}

/* throws error with a message */
void throw_error(char *message) {
	fprintf(stderr, "%s", message);
	exit(EXIT_FAILURE);
}

/* opens a file and sets it to the sepcified file_descr */
void configure_file(char * filename, int oflag, int file_descr) {
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	int file = open(filename, oflag, mode);
	if (file < 0) {
		fprintf(stderr, "Failed to open %s\n", filename);
		exit(EXIT_FAILURE);
		
	}
	if (dup2(file, file_descr) == -1) {
		throw_error("error: could not modify a file descriptor table\n");
	}
	close(file);
}

/* configures an input file */
void configure_in_file(char * filename) {
	configure_file(filename, O_RDONLY, STDIN_FILENO); }

/* configures an output file */
void configure_out_file(char * filename) {
	configure_file(filename, O_WRONLY | O_CREAT | O_TRUNC, STDOUT_FILENO);
}

/* configures the pipe 'the right side' of a process */
void configure_out_pipe(int *pipe_out) {
	close(pipe_out[READ_SIDE]);
	if (dup2(pipe_out[WRITE_SIDE], STDOUT_FILENO) == -1) {
		throw_error("error: could not modify a file descriptor table\n");
	}
	close(pipe_out[WRITE_SIDE]);
}

/* configures the pipe 'on the left side' of a process */
void configure_in_pipe(int *pipe_in) {
	close(pipe_in[WRITE_SIDE]);
	if (dup2(pipe_in[READ_SIDE], STDIN_FILENO) == -1) {
		throw_error("error: could not modify a file descriptor table\n");
	}
	close(pipe_in[READ_SIDE]);
}

/* Called upon receiving a successful parse.
 * This function is responsible for cycling through the
 * tasks, and forking, executing, etc as necessary to get
 * the job done! */
void execute_tasks (Parse* P)
{
    unsigned int t;
	int fd1[2], fd2[2];
	int num_children = 0;
	JobStatus status = P->background ? BG : FG;
	Job *job = Job_Constructor(job_name, P->ntasks, status);
	JobArray_AddJob(&job_arr, job);

    for (t = 0; t < P->ntasks; t++) {
		if (!strcmp("exit", P->tasks[t].cmd)) {
			builtin_execute(P->tasks[t]);
		}
		else if (command_found (P->tasks[t].cmd) || is_builtin (P->tasks[t].cmd)) {
			num_children ++;
			int *pipe_in, *pipe_out;
			if (t % 2 == 0) { // toggling the file descriptors
				pipe_out = fd1;
				pipe_in = fd2;
			} else {
				pipe_out = fd2;
				pipe_in = fd1;
			}
			if (t < P->ntasks - 1) {
				if (pipe(pipe_out) == -1) { // create a pipe
					fprintf(stderr, "error: failed to create pipe\n");
					return;
				}
			}
			pid_t curr_pid = fork();
			Job_AddPid(job, curr_pid, t); // add the pid to pids[] for the job
			Job_MovePidToProcessGroup(job, t); // move the pid to the Job's process group
			// TODO: Move to foreground
			switch(curr_pid) {
				case -1:
					fprintf(stderr, "error: failed to fork a process\n");
					return;
				case 0:
					if (!P->background) {
						while (tcgetpgrp(STDOUT_FILENO) != getpgrp());
					}
					// pipe on the right:
					if (t < P->ntasks - 1) {
						configure_out_pipe(pipe_out);
					} else if (P->outfile) {
						configure_out_file(P->outfile);
					}
					
					// pipe on the left:
					if (t > 0) {
						configure_in_pipe(pipe_in);
					} else if (P->infile) {
						configure_in_file(P->infile);
					}
					
					// execute the process
					if (is_builtin(P->tasks[t].cmd)) {
						builtin_execute(P->tasks[t]);
					} else {
						execvp(P->tasks[t].cmd, P->tasks[t].argv);
					}
				default:
					break;
			}
			// close the parent file descriptors that are not needed anymore
			if (t < P->ntasks - 1) close(pipe_out[WRITE_SIDE]);
			if (t > 0) close(pipe_in[READ_SIDE]);
        }
        else {
            printf ("pssh: command not found: %s\n", P->tasks[t].cmd);
            break;
        }
    }
	printf("Created process group %d\n", job->pgid);
	if (!P->background && isatty(STDOUT_FILENO)) {
        tcsetpgrp(STDOUT_FILENO, job->pgid);
	} else {
		Job_PrintPids(job);
	}
}


int main (int argc, char** argv)
{
    char* cmdline;
    Parse* P;
	job_arr = JobArray_Constructor();

    print_banner ();
	
	// TODO: Save the old handlers?
    our_tty = dup(STDERR_FILENO);
	signal(SIGTTOU, handler);
	signal(SIGTTIN, handler);
	signal(SIGCHLD, handler);

    while (1) {
		char * prompt = build_prompt();
        cmdline = readline (prompt);
		free(prompt);
        if (!cmdline)       /* EOF (ex: ctrl-d) */
            exit (EXIT_SUCCESS);

        P = parse_cmdline (cmdline);
        if (!P)
            goto next;

        if (P->invalid_syntax) {
            printf ("pssh: invalid syntax\n");
            goto next;
        }

#if DEBUG_PARSE
        parse_debug (P);
#endif

        execute_tasks (P);

    next:
        parse_destroy (&P);
        free(cmdline);
    }
}
