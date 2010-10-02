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

#define HELP_INTRO _(\
"Help commands:\n" \
"  help [<command>]\n" \
"  copying\n" \
"  warranty\n" \
"\n" \
"Schema commands:\n" \
"  catalogs\n" \
"  schemas\n" \
"  tables [<catalog>]\n" \
"  columns <table>\n" \
"\n" \
"Transaction commands:\n" \
"  autocommit on|off\n" \
"  commit\n" \
"  rollback\n" \
"\n" \
"Other commands:\n" \
"  set [<variable>] [<value>]\n" \
"  unset <variable>\n" \
"  info" \
		)

#define HELP_NOTFOUND _("Help topic doesn't exist")

