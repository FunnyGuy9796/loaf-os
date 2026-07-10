#include "include/sys/path.h"
#include "include/string.h"
#include "include/unistd.h"

#define MAX_PATH_DEPTH 16
#define MAX_BUF 128

void path_normalize(const char *in, char *out, size_t out_size) {
    char stack[MAX_PATH_DEPTH][32];
    int depth = 0;

    const char *p = in;

    while (*p) {
        while (*p == '/')
            p++;

        if (!*p)
            break;

        char comp[32];
        int i = 0;

        while (*p && *p != '/' && i < (int)sizeof(comp) - 1)
            comp[i++] = *p++;

        comp[i] = '\0';

        while (*p && *p != '/')
            p++;

        if (strcmp(comp, ".") == 0) {
            continue;
        } else if (strcmp(comp, "..") == 0) {
            if (depth > 0)
                depth--;
        } else if (depth < MAX_PATH_DEPTH) {
            strncpy(stack[depth], comp, sizeof(stack[depth]) - 1);

            stack[depth][sizeof(stack[depth]) - 1] = '\0';
            depth++;
        }
    }

    size_t pos = 0;
    out[pos++] = '/';

    for (int i = 0; i < depth; i++) {
        size_t len = strlen(stack[i]);

        if (pos + len + 2 >= out_size)
            break;

        memcpy(out + pos, stack[i], len);
        pos += len;

        if (i != depth - 1)
            out[pos++] = '/';
    }

    out[pos] = '\0';
}

void path_resolve_cwd(const char *in, char *out, size_t out_size) {
    char combined[MAX_BUF * 2];
    size_t clen;
    char cwd[MAX_BUF];

    getcwd(cwd, (int)MAX_BUF);

    if (in[0] == '/') {
        strncpy(combined, in, sizeof(combined) - 1);

        combined[sizeof(combined) - 1] = '\0';
    } else {
        strncpy(combined, cwd, sizeof(combined) - 1);

        combined[sizeof(combined) - 1] = '\0';
        clen = strlen(combined);

        if (clen > 0 && combined[clen - 1] != '/' && clen < sizeof(combined) - 1) {
            combined[clen] = '/';
            combined[clen + 1] = '\0';
            clen++;
        }

        strncat(combined, in, sizeof(combined) - clen - 1);
    }

    path_normalize(combined, out, out_size);
}