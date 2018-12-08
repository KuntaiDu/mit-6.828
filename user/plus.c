
#include <inc/lib.h>
#include <inc/monitor.h>
#include <inc/env.h>

int page[3] __attribute__((aligned(PGSIZE)));
int common = 0;

void
umain(int argc, char **argv) {

	int whom, perm;

	envid_t monitor = ipc_find_env(ENV_TYPE_MT);
	ipc_send(monitor, MONITOR_MUTEX_INIT, page, PTE_P | PTE_W | PTE_U);
	ipc_recv(&whom, NULL, &perm);
	int lockid = page[0];
	
	int ret = fork(), i, j;
	int envid = sys_getenvid();

	for (i = 1; i <= 10; i ++) {
		ipc_send(monitor, MONITOR_MUTEX_P, page, PTE_P | PTE_U);
		ipc_recv(&whom, NULL, &perm);
		for (j = 1; j <= 10; j ++) {
			common ++;
			printf("from env %d: %d\n", envid, common);
		}
		ipc_send(monitor, MONITOR_MUTEX_V, page, PTE_P | PTE_U);
		ipc_recv(&whom, NULL, &perm);
	} 
}
