/*
    dbsh - text-based ODBC client
    Copyright (C) 2007, 2008 Ben Spencer

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <pthread.h>
#include <signal.h>

#include "common.h"
#include "db.h"
#include "err.h"


pthread_t signal_thread;
sigset_t sig_set;


static void *signal_thread_start()
{
	int sig;

	// TODO: is this necessary, or is it inherited?
	if(pthread_sigmask(SIG_BLOCK, &sig_set, 0)) err_system();

	for(;;) {
		sigwait(&sig_set, &sig);
		db_cancel_query();
	}
}

void signal_handlers_install()
{
	struct sigaction act;

	// Ignore SIGPIPE
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	act.sa_handler = SIG_IGN;
	if(sigaction(SIGPIPE, &act, (struct sigaction *) 0) == -1) err_system();

	// SIGINT handler thread
	sigemptyset(&sig_set);
	sigaddset(&sig_set, SIGINT);
	if(pthread_sigmask(SIG_BLOCK, &sig_set, 0)) err_system();
	if(pthread_create(&signal_thread, 0, signal_thread_start, 0)) err_system();
}
