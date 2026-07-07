#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define EXE_MAGIC 0x4c4f4146u

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <flat.bin> <code_vaddr_hex> <out.bin>\n", argv[0]);

        return 1;
    }

    FILE *in = fopen(argv[1], "rb");

    if (!in) {
        perror("fopen input");
        
        return 1;
    }

    fseek(in, 0, SEEK_END);

    long code_size = ftell(in);

    fseek(in, 0, SEEK_SET);

    uint8_t *code = malloc(code_size);

    fread(code, 1, code_size, in);
    fclose(in);

    uint32_t code_vaddr = strtoul(argv[2], NULL, 16);
    uint32_t hdr[8] = {
        EXE_MAGIC,
        code_vaddr,
        code_vaddr,
        (uint32_t)code_size,
        0, 0,
        0, 0x1000
    };

    FILE *out = fopen(argv[3], "wb");

    if (!out) {
        perror("fopen output");
        
        return 1;
    }

    fwrite(hdr, sizeof(hdr), 1, out);
    fwrite(code, code_size, 1, out);
    fclose(out);
    free(code);

    return 0;
}