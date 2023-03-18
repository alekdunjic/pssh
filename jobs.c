#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <unistd.h>

#include "jobs.h"

// TODO: Handle all the malloc errors
void set_fg_pgrp(pid_t pgrp)
{
    void (*sav)(int sig);

    if (pgrp == 0)
        pgrp = getpgrp();

    sav = signal(SIGTTOU, SIG_IGN);
    tcsetpgrp(our_tty, pgrp);
    signal(SIGTTOU, sav);
}

/* return a Job pointer on the heap */
Job *Job_Constructor(char *name, unsigned int npids, JobStatus status) {
	Job *job = malloc(sizeof(Job));
	job->num = -1;
	job->name = malloc(sizeof(char) * (strlen(name) + 1));
	strcpy(job->name, name);
	job->pids = malloc(npids*sizeof(pid_t));
	job->npids = npids;
	job->pgid = 0;
	job->status = status;
	job->finished_pids = 0;
	return job;
}

/* deallocates a job */
void Job_Destructor(Job *job) {
	free(job->name);
	free(job->pids);
	free(job);
}

/* adds a pid to a job to index t */
void Job_AddPid(Job *job, pid_t pid, unsigned int t) {
	job->pids[t] = pid;
}

/* get a pid from index t */
pid_t Job_GetPid(Job *job, unsigned int t) {
	return job->pids[t];
}

/* checks if a job contains a specified pid */
int Job_HasPid(Job *job, pid_t pid) {
	int i;
	for (i = 0; i < job->npids; i++) {
		if (job->pids[i] == pid) {
			return 1;
		}
	}
	return 0;
}

/* move the pid at index t to appropriate process group */
void Job_MovePidToProcessGroup(Job *job, unsigned int t) {
	job->pgid = job->pids[0];
	setpgid(job->pids[t], job->pgid);
}

/* checks if all pid's in a job are done */
int Job_IsFinished(Job* job) {
	return job->npids == job->finished_pids;
}

/* print the job PIDs */
void Job_PrintPids(Job *job) {
	printf("[%d] ", job->num);
	int i;
	for(i = 0; i < job->npids; i++) {
		printf("%d ", job->pids[i]);
	}
	printf("\n");
}

/* returns a JobArray (copy to stack) */
JobArray JobArray_Constructor() {
	JobArray job_arr;
	job_arr.jobs = malloc(100*sizeof(Job *));
	int i;
	for (i = 0; i < 100; i++) {
		job_arr.jobs[i] = NULL;
	}
	job_arr.njobs = 0;
	job_arr.curr_fg = -1;
	return job_arr;
}

/* deallocates everything that a JobArray owns */
void JobArray_Destructor(JobArray *job_arr) {	
	int i;
	for (i = 0; i < 100; i++) {
		if(job_arr->jobs[i] != NULL) {
			Job_Destructor(job_arr->jobs[i]);
			return;
		}
	}
	free(job_arr->jobs);
}

/* adds a job to the first avaliable spot in the JobArray */
void JobArray_AddJob(JobArray *job_arr, Job *job) {
	int i;
	for (i = 0; i < 100; i++) {
		if(job_arr->jobs[i] == NULL) {
			job->num = i;
			job_arr->jobs[i] = job;
			job_arr->njobs++;
			return;
		}
	}
}

/* remove a job based on the job index */
void JobArray_RemoveJob(JobArray *job_arr, int j) {
	Job_Destructor(job_arr->jobs[j]);
	job_arr->jobs[j] = NULL;
	job_arr->njobs--;
}

/* increment finished_pid for a job 
 * return an index from JobArray if the job is done
 * otherwise, return -1 */
int JobArray_HandleChild(JobArray *job_arr, pid_t pid) {
	int i;
	for (i = 0; i < 100; i++) {
		if (job_arr->jobs[i] != NULL && Job_HasPid(job_arr->jobs[i], pid)) {
			job_arr->jobs[i]->finished_pids++;
			if (Job_IsFinished(job_arr->jobs[i])) {
				return i;
			}
			break;
		}
	}
	return -1;
}

int JobArray_FindJob(JobArray *job_arr, pid_t pid) {
	int i;
	for (i = 0; i < 100; i++) {
		if (job_arr->jobs[i] != NULL && Job_HasPid(job_arr->jobs[i], pid)) {
			return i;
		}
	}
	return -1;
}

void Job_PrintJob(Job *job) {
	char stopped_str[] = "stopped";
	char running_str[] = "running";
	char *status_str;
	if(job->status == STOPPED) {
		status_str = stopped_str;
	} else {
		status_str = running_str;
	}
	printf("[%d] + %s \t %s\n", job->num, status_str, job->name);
}

void JobArray_PrintJobs(JobArray *job_arr) {
	int i;
	for (i = 0; i < 100; i++) {
		if (job_arr->jobs[i] != NULL) {
			Job_PrintJob(job_arr->jobs[i]);
		}
	}
}

void JobArray_MoveToFg(JobArray *job_arr, int job_num) {
	if (job_num == -1) {
		set_fg_pgrp(0);
	} else {
		job_arr->jobs[job_num]->status = FG;
		set_fg_pgrp(job_arr->jobs[job_num]->pgid);
		killpg(job_arr->jobs[job_num]->pgid, SIGCONT);
	}
	/* update the status of the old FG to BG */
	if (job_arr->curr_fg != -1 && job_arr->jobs[job_arr->curr_fg] != NULL) {
		job_arr->jobs[job_arr->curr_fg]->status = BG;
	}
	job_arr->curr_fg = job_num;
}

void JobArray_MoveToBg(JobArray *job_arr, int job_num) {
	job_arr->jobs[job_num]->status = BG;
	killpg(job_arr->jobs[job_num]->pgid, SIGCONT);
}
