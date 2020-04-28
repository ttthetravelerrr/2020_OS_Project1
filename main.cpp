#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<string>
#include<algorithm>
#include<vector>
#include<unistd.h>
#include<sys/syscall.h>
#include<sys/time.h>
#include<sys/wait.h>
#include<sched.h>
#include<queue>
using namespace std;


#define waitUnitTime { volatile unsigned long i; for(i=0;i<1000000UL;i++);}

class Process{
public:
	std::string name;
	int readyTime;
	int execTime;
	int pid;
	int runTime;
	Process() {}
	Process(const std::string &n, int r ,int e) : name(n), readyTime(r), execTime(e){
		runTime = 0;
	}
	Process(const std::string &n, int r ,int e, int p) : name(n), readyTime(r), execTime(e), pid(p){
		runTime = 0;
	}
};

double getTime();
bool comp(Process P1, Process P2);
bool compexec(Process P1, Process P2);
void putToSleep(int pid);
void wakeUp(int pid);
static int FIFOspawn(const Process& P);
static int RRspawn(const Process& P);

void FIFO(int processNum, vector<Process> processList);
void RR(int processNum, vector<Process> processList);
void SJF(int processNum, vector<Process> processList);
void PSJF(int processNum, vector<Process> processList);


int main(){
	char inputstr[8];
	scanf("%s",inputstr);
	int processNum;
	scanf("%d",&processNum);
	vector<Process> processList;
	for(int i=0; i<processNum ; i++){
		char buf[8];
		int rt,et;
		scanf("%s %d %d",buf,&rt,&et);
		processList.emplace_back(buf,rt,et);
	}
	if(strcmp(inputstr,"FIFO")==0)
		FIFO(processNum,processList);
	else if(strcmp(inputstr,"RR")==0)
		RR(processNum,processList);
	else if(strcmp(inputstr,"SJF")==0)
		SJF(processNum,processList);
	else if(strcmp(inputstr,"PSJF")==0)
		PSJF(processNum,processList);
	return 0;
}

double getTime(){
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME,&ts);
	return ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
bool comp(Process P1, Process P2){
	if(P1.readyTime == P2.readyTime)
		return P1.execTime < P2.execTime;
	else
		return P1.readyTime < P2.readyTime;
}

bool compexec(Process  P1, Process  P2){
	if(P1.execTime == P2.execTime)
		return P1.readyTime < P2.readyTime;
	else
		return P1.execTime < P2.execTime;
}
void putToSleep(int pid){
	struct sched_param param;
	param.sched_priority = 0;
	if(sched_setscheduler(pid,SCHED_IDLE,&param) != 0)
		fprintf(stderr,"sleep rr set sched failed\n");
}
void wakeUp(int pid){
	struct sched_param param;
	param.sched_priority = 99;
	if(sched_setscheduler(pid,SCHED_FIFO,&param) != 0)
		fprintf(stderr,"wake %d rr set sched failed\n", pid);

}

int FIFOspawn(Process& P){
	int pid;
	if(pid = fork()){
		//Parent  here
		if(pid == -1) 
			fprintf(stderr,"fifo.cpp line 53 fork error\n");
		return pid;
	}
	else{
		//child  here 
		double start = getTime();
		//fprintf(stderr,"spawned %s %d\n", P.name.c_str(),getpid());
		printf("%s %d\n",P.name.c_str(),getpid());
		cpu_set_t cpuMask;
		CPU_ZERO(&cpuMask);
		CPU_SET(1,&cpuMask);
		if(sched_setaffinity(getpid(),sizeof(cpu_set_t),&cpuMask)!=0)
			fprintf(stderr,"line 86 fail\n");
		int execTime = P.execTime;
		while(execTime > 0){
			waitUnitTime;
			execTime--;
		}
		double end = getTime();
		char buf[1000];
		int len = snprintf(buf,1000,"[Project 1] %d %.9f %.9f",getpid(),start,end);
		syscall(334,buf,len);
		exit(0);


	}
}


void PSJF(int processNum, vector<Process> processList){
	sort(processList.begin(),processList.end(),comp);
	vector<Process> execList;
	cpu_set_t cpuMask;
	CPU_ZERO(&cpuMask);
	CPU_SET(0,&cpuMask);
	if (sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpuMask) != 0) 
		fprintf(stderr,"FIFO affinity error");
	int T = 0;
	int PRemain = processNum;
	int spawned = 0;
	int running = 0;
	int upTime = 0;
	int flag = 0;
	while(PRemain > 0){
		while(flag < processNum){
			if(processList[flag].readyTime <= T){
				Process & P = processList[flag];
				P.pid = FIFOspawn(P);
				execList.emplace_back(P.name,P.readyTime,P.execTime,P.pid);
				putToSleep(P.pid);
				flag++;
				spawned ++;
				if(running){
					//fprintf(stderr,"before %d %d\n",execList[0].pid,execList[0].execTime);
					execList[0].execTime -= upTime;
					upTime = 0;
					//fprintf(stderr,"during %d %d\n",execList[0].pid,execList[0].execTime);
					putToSleep(execList[0].pid);
					sort(execList.begin(),execList.end(),compexec);
					wakeUp(execList[0].pid);
					//fprintf(stderr,"after %d %d\n",execList[0].pid,execList[0].execTime);
					running = execList[0].pid;
				}
			}
			else
				break;
		}
		if(running){
			int pid = running;
			if(waitpid(pid,NULL,WNOHANG) != 0){
				running = 0;
				PRemain--;
				spawned--;
				upTime = 0;
				execList.erase(execList.begin());
				//fprintf(stderr,"premain = %d\n",PRemain);
			}
		}
		if(!running && spawned > 0){
			sort(execList.begin(),execList.end(),compexec);
			wakeUp(execList[0].pid);
			running = execList[0].pid;
//			execList.erase(execList.begin());
		}
		waitUnitTime;
		T++;
		if(running)
			upTime ++;
	}

}
void SJF(int processNum, vector<Process> processList){
	sort(processList.begin(),processList.end(),comp);
	vector<Process> execList;
	cpu_set_t cpuMask;
	CPU_ZERO(&cpuMask);
	CPU_SET(0,&cpuMask);
	if (sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpuMask) != 0) 
		fprintf(stderr,"FIFO affinity error");
	int T = 0;
	int PRemain = processNum;
	int spawned = 0;
	int running = 0;
	int flag = 0;
	while(PRemain > 0){
		while(flag < processNum){
			if(processList[flag].readyTime <= T){
				Process & P = processList[flag];
				P.pid = FIFOspawn(P);
				execList.emplace_back(P.name,P.readyTime,P.execTime,P.pid);
				putToSleep(P.pid);
				flag++;
				spawned ++;
			}
			else
				break;
		}
		if(running){
			int pid = running;
			if(waitpid(pid,NULL,WNOHANG) != 0){
				running = 0;
				PRemain--;
				spawned--;
				//fprintf(stderr,"premain = %d\n",PRemain);
			}
		}
		if(!running && spawned > 0){
			sort(execList.begin(),execList.end(),compexec);
			wakeUp(execList[0].pid);
			running = execList[0].pid;
			execList.erase(execList.begin());
		}
		waitUnitTime;
		T++;
	}

}
void RR(int processNum, vector<Process> processList){
	sort(processList.begin(), processList.end(), comp);
	cpu_set_t cpuMask;
	CPU_ZERO(&cpuMask);
	CPU_SET(0,&cpuMask);
	if (sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpuMask) != 0) 
		fprintf(stderr,"RR affinity error");
	queue<int> Q;
	int T = 0;
	int upTime = 0;
	int running = 0;
	int flag = 0;
	int PRemain = processNum;
	while (PRemain > 0){
		while(flag < processNum){
			if(processList[flag].readyTime <= T){
				Process & P = processList[flag];
				int pid = FIFOspawn(P);
				//fprintf(stderr,"this one \n");
				putToSleep(pid);
				//fprintf(stderr,"this one \n");
				Q.push(pid);
				flag++;
			}
			else
				break;
		}
		if(running){
			int pid = running;
			//fprintf(stderr,"pidnum = %d\n",pid);
			if(waitpid(pid,NULL,WNOHANG) == 0){
				if(upTime >= 500){
					//fprintf(stderr,"that one \n");
					putToSleep(pid);
					//fprintf(stderr,"that one \n");
					Q.push(pid);
					running = 0;
				}
			}
			else{
				//fprintf(stderr,"%d fk\n",T);
				running = 0;
				PRemain--;
			}
		}
		if(!running){
			if(!Q.empty()){

				int pid = Q.front();
				//fprintf(stderr,"pidnum = %d\n",pid);
				Q.pop();
				running = pid;
				wakeUp(pid);
			}
		}
		if(upTime >=500)
			upTime = 0;
		waitUnitTime;
		T++;
		upTime++;
	}
}
void FIFO(int processNum, vector<Process> processList){
	sort(processList.begin(), processList.end(), comp);

	cpu_set_t cpuMask;
	CPU_ZERO(&cpuMask);
	CPU_SET(0,&cpuMask);
	if (sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpuMask) != 0) 
		fprintf(stderr,"FIFO affinity error");
	int T = 0;
	for(int i=0; i<processNum; i++){
		Process & P  = processList[i];
		while(P.readyTime > T){
			waitUnitTime;
			T++;
		}
		struct sched_param param;
		param.sched_priority = 99-i;
		P.pid = FIFOspawn(P);
		if(sched_setscheduler(P.pid, SCHED_FIFO, &param) != 0)
			fprintf(stderr," line 129 error\n");
	}
	for(int i=0 ; i<processNum ; i++)
		wait(NULL);
}
