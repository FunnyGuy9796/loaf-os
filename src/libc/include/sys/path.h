#ifndef SYS_PATH_H
#define SYS_PATH_H

#include <stdint.h>
#include <stddef.h>

void path_normalize(const char *in, char *out, size_t out_size);
void path_resolve_cwd(const char *in, char *out, size_t out_size);

#endif