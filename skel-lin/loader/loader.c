/*
 * Loader Implementation
 *
 * 2018, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "exec_parser.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

static so_exec_t *exec;
static int fd;

void segv_handler(int signum, siginfo_t *info, void *context)
{
	int page;
	char *p;

	for (int i = 0; i < exec->segments_no; i++) {
		if ((int)info->si_addr >= ((exec->segments) + i)->vaddr &&
			(int)info->si_addr <= ((exec->segments) + i)->vaddr + ((exec->segments) + i)->mem_size) {
			so_seg_t *curr = (exec->segments) + i;
			page = ((int)info->si_addr - curr->vaddr) / getpagesize();

			if (!curr->data)
				curr->data = malloc(curr->mem_size / getpagesize() * sizeof(char));

			if (((char *)(curr->data))[page])
				raise(SIGSEGV);
			// Map the page
			p = mmap((void *)(curr->vaddr + page * getpagesize()), getpagesize(), PROT_WRITE, MAP_SHARED | MAP_ANON, fd, 0);

			if (p == MAP_FAILED)
				exit(EXIT_FAILURE);

			((char *)(curr->data))[page] = 1;

			if (page * getpagesize() < curr->file_size) {
				lseek(fd, curr->offset + page * getpagesize(), SEEK_SET);

				if (!(read(fd, p, MIN(curr->file_size - page * getpagesize(), getpagesize()))))
					exit(EXIT_FAILURE);
			}
			// Set permissions
			mprotect(p, getpagesize(), curr->perm);
			return;
		}
	}
	signal(SIGSEGV, SIG_DFL);
}

int so_init_loader(void)
{
	struct sigaction action;

	exec = malloc(sizeof(so_exec_t));
	action.sa_sigaction = segv_handler;
	sigaction(SIGSEGV, &action, NULL);

	return 0;
}

int so_execute(char *path, char *argv[])
{
	fd = open(path, O_RDONLY);
	exec = so_parse_exec(path);
	if (!exec)
		return -1;

	so_start_exec(exec, argv);
	close(fd);
	return -1;
}
