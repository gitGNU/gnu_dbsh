#include <signal.h>
#include <stdio.h>

#include "db.h"


static void signal_handler(int signum)
{
	cancel_query();
}

void signal_handler_install()
{
	struct sigaction act;

	act.sa_handler = signal_handler;
	act.sa_flags   = 0;

	sigemptyset(&act.sa_mask);

	if(sigaction(SIGINT, &act, (struct sigaction *) 0) == -1) {
		perror("Failed to set signal handler");
	}
}
