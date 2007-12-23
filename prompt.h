#ifndef PROMPT_H
#define PROMPT_H

#include "buffer.h"

const char *prompt_render(SQLHDBC, sql_buffer *);

#endif
