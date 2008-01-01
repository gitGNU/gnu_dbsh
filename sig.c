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

#include <signal.h>
#include <stdio.h>

#include "common.h"
#include "db.h"


static void signal_handler(int signum)
{
	cancel_query();
}

void signal_handler_install()
{
	struct sigaction act;

	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);

	act.sa_handler = signal_handler;
	if(sigaction(SIGINT, &act, (struct sigaction *) 0) == -1)
		perror("Failed to set signal handler");

	act.sa_handler = SIG_IGN;
	if(sigaction(SIGPIPE, &act, (struct sigaction *) 0) == -1)
		perror("Failed to set signal handler");
}
