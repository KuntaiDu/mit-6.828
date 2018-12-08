
#include <inc/lib.h>
#include <inc/env.h>
#include <inc/monitor.h>

struct Linklist {
	envid_t envid;
	struct Linklist *next;
};


/*
 * This value is used to receive the parameters from ipc_send.
 */
void *param = (int *) 0x0ffff000;

/*
 * Store the value of i^th lock at lock[i].
 * Binary value, 0 initially
 * Can be initialized to 1 by MONITOR_MUTEX_INIT
 */
int lock[NLOCK], nlock;

/*
 * Store env_id of waiting processes.
 * The processes waiting for lock i are:
 * 	head[i] -> head[i].next -> ... -> NULL
 */
int nnode;
struct Linklist node[NNODE];
struct Linklist *head[NLOCK];



/*
 * Initialize the monitor
 */
static void
monitor_init(void);


/*
 * Pend a new process envid that waiting for lock[id]
 */
static void
pend(envid_t envid, int id);


/*
 * Pop a process that waiting for lock[id]
 */
static envid_t
pop(int id);


/*
 * Check whether envid already requiring for lock[id]
 */
static int
check(envid_t envid, int id);


/*
 * Return a lock initialized with value 1
 */
static void
mutex_init(envid_t envid, int *id);


/*
 * Require lock[id]
 */
static void
mutex_p(envid_t envid, int *id);


/*
 * Release lock[id]
 */
static void
mutex_v(envid_t envid, int *id);


typedef void (* sync_handler)(envid_t, int *);
sync_handler handlers[] = {
	[MONITOR_MUTEX_INIT]	=	mutex_init,
	[MONITOR_MUTEX_P]	=	mutex_p,
	[MONITOR_MUTEX_V]	=	mutex_v
};



/*
 * Main loop
 */
void
umain(int argc, char **argv) {


	cprintf("Spawning monitor with envid %d\n", thisenv->env_id);

	uint32_t req, whom;
	int perm;

	monitor_init();

	while (1) {
		req = ipc_recv((int32_t *)&whom, param, &perm);
		handlers[req]((envid_t) whom, (int *) param);
	}
}

/*
 * Initialize the monitor
 */
static void
monitor_init(void) {
	// No lock and stunned environment yet.
	nlock = nnode = -1;

	// Initialize array lock, node, head
	int i;
	for (i = 0; i < NLOCK; i ++) {
		lock[i] = 0;
		head[i] = NULL;
	}

	for (i = 0; i < NNODE; i ++) {
		node[i].envid = 0;
		node[i].next = NULL;
	}
}

/*
 * Pend a new process that waiting for lock[id]
 */
static void
pend(envid_t envid, int id) {

	// If this process already requiring for this lock, just return
	if (check(envid, id)) {
		return;
	}

	// Allocate a new node
	++ nnode;
	node[nnode].envid = envid;
	node[nnode].next = head[id];
	head[id] = &node[nnode];
}


/*
 * Pop a new process that waiting for lock[id]
 */
static envid_t
pop(int id) {
	envid_t ret = 0;
	
	// Check if there is a waiting process.
	if (head[id] == NULL) {
		ret = -1;
	}
	else {
		ret = head[id]->envid;
		head[id] = head[id]->next;
	}

	return ret;
}

/*
 * Check whether envid already requires lock[id]
 */
static int
check(envid_t envid, int id) {
	struct Linklist *p = head[id];
	while (p != NULL) {
		if (p->envid == envid) {
			return 1;
		}
		p = p->next;
	}
	return 0;
}


/*
 * Return a lock initialized with value 1
 */
static void
mutex_init(envid_t envid, int *id) {
	lock[++nlock] = 1;
	*id = nlock;
	ipc_send(envid, 0, NULL, 0);
}


/*
 * Requiring lock[id]
 */
static void
mutex_p(envid_t envid, int *id) {

	// Avoiding multiple memory access
	int lockid = *id;

	if (lock[lockid] == 1) {	// If this lock has not been occupied
		lock[lockid] = 0;
		ipc_send(envid, 0, NULL, 0);	// Let this process continue
	}
	else {
		pend(envid, lockid);	// Stop
	}
}


/*
 * Releasing lock[id]
 */
static void
mutex_v(envid_t envid, int *id) {

	// Avoiding multiple memory access
	int lockid = *id;

	ipc_send(envid, 0, NULL, 0);

	envid = pop(lockid);
	if (envid < 0) {	// No waiting process
		lock[lockid] =1 ;
	}
	else {	// Other processes waiting
		ipc_send(envid, 0, NULL, 0);
	}
}
