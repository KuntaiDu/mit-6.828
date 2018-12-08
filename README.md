# Monitor realization based on mit-6.828

## Framework
Use monitor process to realize functions for synchronization (e.g. lock). The monitor process is completely sequential. Therefore, 
the synchronization requirements can be easily met by trap into the monitor process.

## Pipeline
The process requires for synchronization $\rightarrow$ trap into monitor process by ipc communication $\rightarrow$ the monitor process 
deal with this issue $\rightarrow$ the monitor process thinks that the process is allowed to continue running $\rightarrow$ notify the
process by ipc communication $\rightarrow$ the process continue running.

## Usage
The monitor process runs right after booting with environment type ENV_TYPE_MT. The process that desires synchronization find the monitor 
process by 
```C
ipc_find_env(ENV_TYPE_MT);
```
and then use ipc_send and ipc_recv to trap into monitor for synchronization.

## How to see that the monitor process works?
I implement a simple test called plus. Run
```bash
make qemu
```
Then run
```bash
plus
```
Plus is a process that forks a new process. These two processes prints 1,2,...,100 in turn. We want to make sure that
each group in 1,...,10;2,...,20;...;91,...,100 is printed as a whole. This is realized by the P operation
``` C
ipc_send(monitor, MONITOR_MUTEX_P, page, PTE_P | PTE_U);
ipc_recv(&whom, NULL, &perm);
```
and V operation
```
ipc_send(monitor, MONITOR_MUTEX_V, page, PTE_P | PTE_U);
ipc_recv(&whom, NULL, &perm);
```
