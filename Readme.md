# VCPU Scheduler

This document outlines the algorithm used by the VCPU scheduler to balance CPU utilization across physical CPUs (pCPUs) in a virtualized environment.
 

### Steps 
After following the instructions present in Project Readme folder, below are the steps that shows the algorithm implementation

### 1. Measure CPU Utilization of Every Domain
- The scheduler measure the CPU utilization of each domain by tracking the previous CPU time and subtracting it from the current CPU time.
- The `virDomainGetVcpus` API is used to retrieve the CPU time and CPU number for each domain.
- CPU utilization is calculated as:
$\text{cpu\_utilization} = \frac{\text{curr\_cpu\_time} - \text{prev\_cpu\_time}}{10^7}$
- This utilization is stored for each domain.



### 2. Calculate Standard Deviation
- The standard deviation of the pCPU utilizations is calculated.
- If the standard deviation is below a threshold (5 in this case), the scheduler concludes that the CPU utilization is balanced and no further action is needed.
- If the standard deviation is above the threshold, the scheduler proceeds to rebalance the CPU utilization.

### 3. Calculate Mean CPU Utilization
- The mean CPU utilization across all domains is calculated.
- The target utilization range for each pCPU is set to be within $\pm 5$ of the mean utilization:
  
  $\text{min\_utilization} = \text{mean\_utilization} - 5$

  $\text{max\_utilization} = \text{mean\_utilization} + 5$

### 4. Sort Domains by CPU Utilization
- The domains are sorted in increasing order of their CPU utilization.
- This helps in assigning domains to pCPUs in a way that balances the load.

### 5. Assign Domains to pCPUs
- The scheduler iterates over each pCPU and assigns domains to it until the pCPU's utilization falls within the target range. Note that, while assigning we are only considering the domains that will be pinned to it in the future and not the ones that are already allocated. That means the initial utilization is 0 for every pCPU for this step.
- The assignment is done by starting from both ends of the sorted list (low load and high load) and adding domains to the pCPU until the utilization is within the desired range - [min_utilization, max_utilization]
  - If adding a domain from the low end does not bring the utilization within range, a domain from the high end is added, and vice versa.
- Once a pCPU's utilization is within the target range, the domains are pinned to that pCPU using the `virDomainPinVcpu` API.

### 6. Handle Remaining Domains
- If there are domains that have not been assigned to any pCPU (i.e., their utilization was not within the target range for any pCPU), they are assigned to the pCPU with the lowest current utilization.

## Key Features
- **Balanced Schedule** - If the standard deviation is less than equal to 5, we aren't running the pin strategy. 
- **Stable Schedule**- The pinnings are done such that low load and high load are combined, this ensures that no extra pinning have to be done. 
