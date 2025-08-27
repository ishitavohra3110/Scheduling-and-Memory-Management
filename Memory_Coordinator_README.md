# Memory Coordinator

This document outlines the algorithm used by the Memory Coordinator to ensure memory usage across domains.

### Steps

After following the instructions present in Project Readme folder, below are the steps that shows the algorithm implementation

### 1. Track Memory Statistics for Each Domain
- The scheduler tracks memory statistics for each domain (VM) using the `virDomainMemoryStats` API.
- Two key memory metrics are monitored:
  - **`VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON`**: Represents the current ballon value of the VM
  - **`VIR_DOMAIN_MEMORY_STAT_UNUSED`**: Represents the completely unused memory within the VM.
- These metrics are tracked across iterations to determine changes in memory usage.

### 2. Determine Memory Needs for Each Domain
- Based on the changes in memory statistics, the scheduler determines whether a domain needs more memory or can relinquish memory.
- **Memory Need Conditions**:
  - If `actual_memory_change > 5 MB` and `actual_memory_change>unused_memory_change` (balloon is increasing), the domain needs more memory.
  - If `actual_memory_change == 0` and `unused_memory_change` is within the range `[-1 MB, -80 MB]`, the domain is starting to consume memory and should not relinquish it yet.
- **Memory Reclaim Conditions**:
  - If `actual_memory_change < -5 MB` (balloon is decreasing), the domain can relinquish memory.
  - If `actual_memory_change == 0` and `unused_memory >= 400 MB`, the domain has excess memory that can be reclaimed.

### 3. Allocate Memory to Domains in Need
- The scheduler calculates the total free memory available on the hypervisor using `virNodeGetFreeMemory`.
- Memory is allocated to domains in need while ensuring the hypervisor does not go below a **200 MB threshold**.
- Each domain in need is allocated **80 MB** of extra memory, but the total allocation is capped at **2048 MB** per domain to prevent over-allocation.
- If a domain's unused memory is below **150 MB**, additional memory is allocated to ensure it stays above this threshold.

### 4. Reclaim Memory from Domains with Excess
- The scheduler reclaims memory from domains that do not need it.
- Each domain with excess memory relinquishes **40 MB** of memory, but the domain's memory is never reduced below **512 MB** to ensure that **512 MB** is always met

### 5. Update Memory Statistics
- After memory allocation or reclamation, the scheduler updates the memory statistics for each domain to reflect the changes using `virDomainSetMemory`

---

## Key Features
- **Threshold-Based Decisions**: Memory adjustments are made only when usage crosses predefined thresholds, memory release is happening gradually **80MB** released from the hypervisor or **40MB** reclaimed.
- **Hypervisor Protection**: Ensures the hypervisor always retains a minimum of **200 MB** of free memory.
- **VM Protection**: Ensures VMs never go below **512 MB** of memory and maintain at least **150 MB** of unused memory.
