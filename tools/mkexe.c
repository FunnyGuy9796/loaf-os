#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define EXE_MAGIC 0x46414f4cu

typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version, e_entry, e_phoff, e_shoff, e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} elf32_hdr_t;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
} elf32_shdr_t;

#define SHT_NOBITS 8

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: %s <flat.bin> <elf.elf> <code_vaddr_hex> <out.bin>\n", argv[0]);

        return 1;
    }

    FILE *elf = fopen(argv[2], "rb");

    if (!elf) {
        perror("fopen elf");
        
        return 1;
    }

    elf32_hdr_t eh;

    if (fread(&eh, sizeof(eh), 1, elf) != 1) {
        fprintf(stderr, "failed to read ELF header\n");

        return 1;
    }

    elf32_shdr_t *shdrs = malloc(eh.e_shnum * sizeof(elf32_shdr_t));

    fseek(elf, eh.e_shoff, SEEK_SET);

    if (fread(shdrs, sizeof(elf32_shdr_t), eh.e_shnum, elf) != eh.e_shnum) {
        fprintf(stderr, "failed to read section headers\n");

        return 1;
    }

    elf32_shdr_t *shstrtab_hdr = &shdrs[eh.e_shstrndx];
    char *shstrtab = malloc(shstrtab_hdr->sh_size);

    fseek(elf, shstrtab_hdr->sh_offset, SEEK_SET);
    fread(shstrtab, 1, shstrtab_hdr->sh_size, elf);

    uint32_t lowest_addr = 0xffffffff;
    uint32_t data_start_addr = 0xffffffff;
    uint32_t data_end_addr = 0;
    uint32_t bss_start_addr = 0xffffffff;
    uint32_t bss_size = 0;
    uint32_t data_file_size = 0;

    uint32_t code_region_end = 0;

    for (int i = 0; i < eh.e_shnum; i++) {
        elf32_shdr_t *sh = &shdrs[i];

        if (sh->sh_addr == 0)
            continue;

        const char *name = shstrtab + sh->sh_name;

        if (sh->sh_addr < lowest_addr)
            lowest_addr = sh->sh_addr;

        if (strcmp(name, ".data") == 0) {
            data_start_addr = sh->sh_addr;
            data_end_addr = sh->sh_addr + sh->sh_size;
            data_file_size = sh->sh_size;
        } else if (strcmp(name, ".bss") == 0) {
            bss_start_addr = sh->sh_addr;
            bss_size = sh->sh_size;
        } else if (sh->sh_flags & 0x2) {
            uint32_t end = sh->sh_addr + sh->sh_size;

            if (end > code_region_end)
                code_region_end = end;
        }
    }

    fclose(elf);
    free(shstrtab);
    free(shdrs);

    uint32_t code_vaddr = strtoul(argv[3], NULL, 16);
    uint32_t code_end_addr = (data_start_addr != 0xffffffff) ? data_start_addr : (bss_start_addr != 0xffffffff) ? bss_start_addr : code_region_end;
    uint32_t code_size = code_end_addr - lowest_addr;
    uint32_t data_vaddr = (data_start_addr != 0xffffffff) ? data_start_addr : 0;
    uint32_t bss_vaddr = (bss_start_addr != 0xffffffff) ? bss_start_addr : (data_end_addr ? data_end_addr : 0);
    FILE *in = fopen(argv[1], "rb");

    if (!in) {
        perror("fopen input");
        
        return 1;
    }

    fseek(in, 0, SEEK_END);

    long flat_size = ftell(in);

    fseek(in, 0, SEEK_SET);

    uint8_t *flat = malloc(flat_size);

    fread(flat, 1, flat_size, in);
    fclose(in);

    uint8_t *code_bytes = flat;
    uint8_t *data_bytes = flat + (data_vaddr - lowest_addr);
    uint32_t hdr[8] = {
        EXE_MAGIC,
        eh.e_entry,
        code_vaddr,
        code_size,
        data_vaddr ? code_vaddr + code_size : 0,
        data_vaddr ? data_file_size : 0,
        bss_vaddr ? code_vaddr + (bss_vaddr - lowest_addr) : 0,
        bss_size
    };

    FILE *out = fopen(argv[4], "wb");

    if (!out) {
        perror("fopen output");
        
        return 1;
    }

    fwrite(hdr, sizeof(hdr), 1, out);
    fwrite(code_bytes, code_size, 1, out);

    if (data_vaddr && data_file_size)
        fwrite(data_bytes, data_file_size, 1, out);

    fclose(out);
    free(flat);

    return 0;
}