#ifndef _jobs_h_
#define _jobs_h_

typedef enum {
	STOPPED,
	TERM,
	BG,
	FG,
} JobStatus;

typedef struct {
	int num;
	char* name;
	pid_t* pids;
	unsigned int npids;
	unsigned int finished_pids;
	pid_t pgid;
	JobStatus status;
} Job;

typedef struct {
	Job **jobs;
	unsigned int njobs;
} JobArray;

JobArray job_arr; // place in the data segment

Job *Job_Constructor(char *name, unsigned int npids, JobStatus status); 
void Job_Destructor(Job *job); 
void Job_AddPid(Job *job, pid_t pid, unsigned int t); 
pid_t Job_GetPid(Job *job, unsigned int t);
int Job_HasPid(Job *job, pid_t pid); 
void Job_MovePidToProcessGroup(Job *job, unsigned int t); 
int Job_IsFinished(Job* job); 
void Job_PrintPids(Job *job); 

JobArray JobArray_Constructor(); 
void JobArray_Destructor(JobArray *job_arr); 
void JobArray_AddJob(JobArray *job_arr, Job *job); 
void JobArray_RemoveJob(JobArray *job_arr, pid_t pgid); 
int JobArray_HandleChild(JobArray *job_arr, pid_t pid); 
void JobArray_PrintJobs(JobArray *job_arr);

#endif /* _jobs_h_ */
