#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "jobs.h"

// TODO: Handle all the malloc errors

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

void JobArray_PrintJobs(JobArray *job_arr) {
	char stopped_str[] = "stopped";
	char running_str[] = "running";
	char *status_str;
	int i;
	for (i = 0; i < 100; i++) {
		if (job_arr->jobs[i] != NULL) {
			if(job_arr->jobs[i]->status == STOPPED) {
				status_str = stopped_str;
			} else {
				status_str = running_str;
			}
			printf("[%d] + %s \t %s\n", i, status_str, job_arr->jobs[i]->name);
		}
	}
}
