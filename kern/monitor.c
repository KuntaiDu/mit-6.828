// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display backtrace information about the stack", mon_backtrace },
	{ "showmappings", "Display the page mappings in range [begin_address, end_address)",
	  mon_showmappings },
	{ "continue", "Continue from the last breakpoint.", mon_continue },
	{ "swapkey", "Swap key", mon_swapkey }
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_swapkey(int argc, char **argv, struct Trapframe *tf) 
{
	change_before_char((int)argv[1][0]);
	change_after_char((int)argv[2][0]);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	
	// First get the position of ebp
	typedef unsigned int *ptr;
	ptr ebp = (ptr) read_ebp();
	cprintf("Stack backtrace:\n");

	// Use this variable to fetch debuginfo
	struct Eipdebuginfo debug_info;

	// Use this variable to store eip
	ptr eip = NULL;

	while (ebp != NULL) {

		// Get eip
		eip = (ptr)*(ebp + 1);

		// Print ebp and eip
		cprintf("ebp %08x ", ebp);
		cprintf("eip %08x ", eip);

		// Print args
		cprintf("args");
		for(int i = 2; i <= 6; i ++) {
			cprintf(" %08x", *(ebp + i));
		}
		cprintf("\n");

		// Get debug_info and print it
		debuginfo_eip((uintptr_t) eip, &debug_info);

		// Note that we need to make debug_info.eip_fn_name shorter
		cprintf("\t%s:%d: %.*s+%d\n",
				debug_info.eip_file,
				debug_info.eip_line,
				debug_info.eip_fn_namelen,
				debug_info.eip_fn_name,
				(uintptr_t) eip - debug_info.eip_fn_addr);

		// Goto upper-level function
		ebp = (ptr) *ebp;

	}

	return 0;
}

int mon_showmappings(int argc, char **argv, struct Trapframe *tf) {
	// Extern variables
	extern pte_t *pgdir_walk(pde_t *pgdir, const void *va, int create);
	extern pde_t *kern_pgdir;

	if (argc != 3) {
		cprintf("Usage: showmappings: begin_address end_address\n");
		return 0;
	}

	long begin = strtol(argv[1], NULL, 16);
	long end = strtol(argv[2], NULL, 16);

	// Sanity check
	assert(begin < end);
	assert(end <= 0xffffffff);
	assert(begin == ROUNDUP(begin, PGSIZE));
	assert(end == ROUNDUP(end, PGSIZE));

	for (; begin < end; begin += PGSIZE) {
		cprintf("%08x--%08x: ", begin, begin + PGSIZE);
		pte_t *pg_tbl_entry = pgdir_walk(kern_pgdir, (void *)begin, 0);
		if (pg_tbl_entry == NULL) {
			cprintf("Not mapped\n");
			continue;
		}
		cprintf("page %08x ", PTE_ADDR(*pg_tbl_entry));
		cprintf("PTE_P: %x, PTE_W: %x, PTE_U: %x\n",
				*pg_tbl_entry & PTE_P,
				*pg_tbl_entry & PTE_W,
				*pg_tbl_entry & PTE_U);
	}

	return 0;
}

int mon_continue(int argc, char **argv, struct Trapframe *tf) {
	// Sanity Check
	if (argc != 1) {
		cprintf("[W]: Do not need any argument.\n");
		return 0;
	}
	// No breakpoint exception?
	if (tf->tf_trapno != T_BRKPT) {
		cprintf("[E]: Cannot continue: no breakpoint exception.\n");
		return 0;
	}
	// Return to trap_dispatch().
	return -1;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
