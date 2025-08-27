#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#define MIN(a, b) ((a) < (b) ? a : b)
#define MAX(a, b) ((a) > (b) ? a : b)

int is_exit = 0; // DO NOT MODIFY THIS VARIABLE
int iteration = 0;
typedef struct vcpu {
	unsigned long long prev_cpu_time;
	unsigned long long curr_cpu_time;
	long double cpu_utilization;
	char  *domain_name; 
	int domain_idx;
	int vcpu_number;
	int cpu_number; //this will be set to something else in the start and will change while pinning.//TODO - check if you need to change while pinning 
} vdomain;
vdomain* virtual_domain;
// comparator that will sort a and b and it will give all the vcpus in ascending order of their time
int vcpuSort(const void *a, const void *b){
	return ((vdomain*) a)->cpu_utilization > ((vdomain*) b)->cpu_utilization;
}

// int physicalCpuSort(const void *a, const void *b){
// 	return ((physicalcpu*) a)->cpu_utilization > ((physicalcpu*) b)->cpu_utilization;
// }
void CPUScheduler(virConnectPtr conn, int interval);

/*
DO NOT CHANGE THE FOLLOWING FUNCTION
*/
void signal_callback_handler()
{
	printf("Caught Signal");
	is_exit = 1;
}

/*
DO NOT CHANGE THE FOLLOWING FUNCTION
*/
int main(int argc, char *argv[])
{
	virConnectPtr conn;

	if (argc != 2)
	{
		printf("Incorrect number of arguments\n");
		return 0;
	}

	// Gets the interval passes as a command line argument and sets it as the STATS_PERIOD for collection of balloon memory statistics of the domains
	int interval = atoi(argv[1]);

	conn = virConnectOpen("qemu:///system");
	if (conn == NULL)
	{
		fprintf(stderr, "Failed to open connection\n");
		return 1;
	}

	// Get the total number of pCpus in the host
	signal(SIGINT, signal_callback_handler);

	while (!is_exit)
	// Run the CpuScheduler function that checks the CPU Usage and sets the pin at an interval of "interval" seconds
	{
		CPUScheduler(conn, interval);
		sleep(interval);
	}

	// Closing the connection
	virConnectClose(conn);
	return 0;
}
long double calculate_std_dev(int num_cpus, long double *pcpu_utilization){

	long double sum = 0;
	for(int i=0;i<num_cpus;i++){
		printf("CPU %d utilization %Lf\n",i,pcpu_utilization[i]);
		sum += (pcpu_utilization[i]);
	}
	long double mean = sum/num_cpus;
	long double var = 0;
	for(int i=0;i<num_cpus;i++)
		var += pow(pcpu_utilization[i]-mean,2);
	var = var/num_cpus;
	return sqrt(var);
}

/* COMPLETE THE IMPLEMENTATION */
void CPUScheduler(virConnectPtr conn, int interval)
{
	printf("new iteration %d\n", iteration);
	virDomainPtr* domains;
	virNodeInfoPtr nodeInfo = malloc(sizeof(virNodeInfo));
	int ret = virNodeGetInfo(conn, nodeInfo); //this will give you the node information
	if(ret < 0){
		printf("Node information not returned\n");
		return;
	}
	int maxCPUs = nodeInfo->cpus;
	long double *pcpu_utilization = (long double*)calloc(maxCPUs,sizeof(long double));
	
	printf("maximum number of cpus active in node %d\n", maxCPUs);
 	unsigned int flags = VIR_CONNECT_LIST_DOMAINS_RUNNING |
                     VIR_CONNECT_LIST_DOMAINS_PERSISTENT; // TODO - check more flags if required
	int num_domains = virConnectListAllDomains(conn, &domains,flags); //step 2 - get all domains that are running
	printf("get number of vms: %d\n",num_domains);
	if(iteration == 0)
	{
		//things have just started, hence assign memory to your virtual_domain;
		virtual_domain = malloc(num_domains*sizeof(vdomain));

	}
	unsigned char cpumaps;
	//print("VCPU MAP",VIR_COPY_CPUMAP);
	int vinfo = 1;
	int maplen = VIR_CPU_MAPLEN(maxCPUs);//8 bits 
	printf("%d maplen size\n", maplen);
	virVcpuInfoPtr vcpuInfo = malloc(num_domains * sizeof(virVcpuInfo));
	printf("Current Domains\n");
	long double total_utilization = 0;
	int domain_num = 0;//check how to sort, the logic should work
	for(int i=0;i<num_domains;i++) // step 3 - get vcpu stats for every domain 
	{
		//vcpuInfo[i] = malloc(maxCpus * sizeof(virVcpuInfo));
		int domain_vcpu = virDomainGetVcpus(domains[i],&vcpuInfo[i],vinfo,&cpumaps,maplen);
		int cpuNumber = vcpuInfo[i].cpu;		
		if(iteration == 0){
			virtual_domain[i].prev_cpu_time = vcpuInfo[i].cpuTime; 
			virtual_domain[i].domain_name = virDomainGetName(domains[i]);
			virtual_domain[i].domain_idx = i;
			virtual_domain[i].curr_cpu_time = vcpuInfo[i].cpuTime;
			virtual_domain[i].cpu_number = vcpuInfo[i].cpu;
			virtual_domain[i].cpu_utilization = (virtual_domain[i].curr_cpu_time-virtual_domain[i].prev_cpu_time)/(long double)1e7;
			virtual_domain[i].vcpu_number = vcpuInfo[i].number;//todo
		}
		//printf("domain number %d utilization %Lf cpu %d state %d prev time %llu\n",i,virtual_domain[i].cpu_utilization,vcpuInfo[i].cpu,vcpuInfo[i].state,vcpuInfo[i].cpuTime);
	}
	printf("Matching Domain Names\n");
	for(int i=0;i<num_domains;i++){
		char* curr_domain_name = virDomainGetName(domains[i]);

		for(int j=0;j<num_domains;j++){ //find the current domain name
			if(strcmp(virtual_domain[j].domain_name,curr_domain_name) == 0){

				virtual_domain[j].domain_idx = i;
				virtual_domain[j].curr_cpu_time = vcpuInfo[i].cpuTime;
				virtual_domain[j].cpu_number = vcpuInfo[i].cpu;
				virtual_domain[j].cpu_utilization = (virtual_domain[j].curr_cpu_time-virtual_domain[j].prev_cpu_time)/(long double)1e7;
				virtual_domain[j].vcpu_number = vcpuInfo[i].number;
				printf("domain number %d match domain number %d utilization %Lf cpu %d  prev time %llu cur time %llu \n",i,j,virtual_domain[j].cpu_utilization,virtual_domain[j].cpu_number,virtual_domain[j].prev_cpu_time,virtual_domain[j].curr_cpu_time);
				virtual_domain[j].prev_cpu_time = vcpuInfo[i].cpuTime;
				total_utilization+=(virtual_domain[j].cpu_utilization);

				pcpu_utilization[virtual_domain[j].cpu_number] += virtual_domain[j].cpu_utilization;
			}
		}
	}
	long double std_dev = calculate_std_dev(maxCPUs,pcpu_utilization);
	if(std_dev <= 5){
		printf("I meet the std dev %Lf, no need to pin new\n",std_dev);
		iteration += 1;
		return;
	}

	long double mean_utilization = (long double)total_utilization/maxCPUs;
	long double min_utilization = (mean_utilization)-5;
	long double max_utilization = (mean_utilization)+5;
	printf("mean utilization : %Lf min_utilization : %Lf max_utilization : %Lf\n",mean_utilization, min_utilization, max_utilization);


	qsort(virtual_domain,num_domains, sizeof(vdomain), vcpuSort);

	//sort every VCPU utilization 
	printf("sorted VCPU times of every VM\n");
	for(int i=0;i<num_domains;i++)
	{
		printf("cpu number %d cpu utilization %Lf curr cpu time %llu\n",virtual_domain[i].cpu_number,virtual_domain[i].cpu_utilization);
	}
	
	//start with minimum cpu time and iterate, then take every
	int start_domain = 0;
	unsigned char cpumap[1] = {0};//TODO - change to left shift 
	int domainDone[num_domains];
	memset(domainDone, 0, num_domains*sizeof(domainDone[0]));
	int low_load = 0;
	int high_load = num_domains-1;
	
	for(int i=0;i<maxCPUs;i++)//physical_cpu
	{
		cpumap[0] = (unsigned char)1<<(i);
		long double new_cpu_utilization = 0;
		
		int temp_low_load = low_load;
		int temp_high_load = high_load;
		while(low_load<high_load){
			new_cpu_utilization += virtual_domain[low_load].cpu_utilization; 
			printf("load %d utilization  %Lf\n",low_load,new_cpu_utilization);
			low_load++;
			if(min_utilization <= new_cpu_utilization && new_cpu_utilization <= max_utilization)
			{
				printf("Domain [%d..%d] will shift to cpu %d and cpu utilization will become %Lf\n",temp_low_load,low_load-1,i,new_cpu_utilization);
				printf("Domain [%d..%d] will shift to cpu %d and cpu utilization will become %Lf\n",high_load+1,temp_high_load,i,new_cpu_utilization);
				for(int j=temp_low_load;j<low_load;j++){
					domainDone[j] = 1;
					virDomainPinVcpu(domains[virtual_domain[j].domain_idx],virtual_domain[j].vcpu_number,cpumap,1);
				}
				for(int j=temp_high_load;j>high_load;j--)
				{
					domainDone[j] = 1;
					virDomainPinVcpu(domains[virtual_domain[j].domain_idx],virtual_domain[j].vcpu_number,cpumap,1);
				}
				pcpu_utilization[i] = new_cpu_utilization;
				temp_low_load = low_load;
				temp_high_load = high_load;
				break;
			}
			new_cpu_utilization += virtual_domain[high_load].cpu_utilization;
			printf("load %d utilization  %Lf\n",high_load,new_cpu_utilization);
			high_load--;

			if(min_utilization <= new_cpu_utilization && new_cpu_utilization <= max_utilization)
			{
				printf("Domain [%d..%d] will shift to cpu %d and cpu utilization will become %Lf\n",temp_low_load,low_load,i,new_cpu_utilization);
				printf("Domain [%d..%d] will shift to cpu %d and cpu utilization will become %Lf\n",high_load+1,temp_high_load,i,new_cpu_utilization);
				for(int j=temp_low_load;j<low_load;j++){
					domainDone[j] = 1;
					virDomainPinVcpu(domains[virtual_domain[j].domain_idx],virtual_domain[j].vcpu_number,cpumap,1);
				}
				for(int j=temp_high_load;j>high_load;j--)
				{
					domainDone[j] = 1;
					virDomainPinVcpu(domains[virtual_domain[j].domain_idx],virtual_domain[j].vcpu_number,cpumap,1);
				}
				pcpu_utilization[i] = new_cpu_utilization;
				temp_low_load = low_load;
				temp_high_load = high_load;
				break;
			}
		}
	}	
	for(int j=0;j<num_domains;j++){
		unsigned long long min_utilization = 0;
		int min_cpu_number = 0;
		if(domainDone[j] == 0)
		{
			for(int k=0;k<maxCPUs;k++){
				if(min_utilization > pcpu_utilization[k])
				{
					min_utilization = pcpu_utilization[k];
					min_cpu_number = k;
				}
			}
			cpumap[0] = (unsigned char)1<<(min_cpu_number);
			pcpu_utilization[min_cpu_number]+=virtual_domain[j].cpu_utilization;
			virDomainPinVcpu(domains[virtual_domain[j].domain_idx],virtual_domain[j].vcpu_number,cpumap,1);
		}
	}
	printf("I scheduled fine\n");
	//repeat - the same cycle for domains not yet done 
	if(start_domain <= (num_domains-1))
		printf("No seg fault till here\n");

	//step - 4 scheduling algorithm
	iteration += 1;
	//exit(0);
	return;

}
