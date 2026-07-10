#include "syscalls.h"
#include "../libc/include/syscall.h"
#include "../vga.h"
#include "../misc/mem.h"
#include "../misc/printf.h"
#include "../multi/process.h"
#include "../devices/pit.h"
#include "../devices/rtc.h"
#include "../devices/kbd.h"
#include "../mm/pmm.h"
#include "../mm/paging.h"
#include "../mm/heap.h"

#define MAX_SPAWN_ARGC 16
#define MAX_SPAWN_ARGLEN 64

static int alloc_fd(process_t *proc) {
    for (int i = 0; i < MAX_FDS_PER_PROC; i++) {
        if (proc->fds[i] == -1)
            return i;
    }

    return -1;
}

static int alloc_dirfd(process_t *proc) {
    for (int i = 0; i < MAX_DIRS_PER_PROC; i++) {
        if (!proc->dirs[i].in_use)
            return i;
    }

    return -1;
}

static int copy_user_str(const char *user_ptr, char *kbuf, uint32_t max) {
    if (!paging_validate_user_range(curr_proc->pd_phys, (uint32_t)user_ptr, 1, 0))
        return -1;

    for (uint32_t i = 0; i < max - 1; i++) {
        uint32_t addr = (uint32_t)user_ptr + i;

        if ((addr & 0xfff) == 0 && !paging_validate_user_range(curr_proc->pd_phys, addr, 1, 0))
            return -1;

        kbuf[i] = user_ptr[i];

        if (kbuf[i] == '\0')
            return 0;
    }

    kbuf[max - 1] = '\0';

    return 0;
}

void syscall_dispatch(registers_t *regs) {
    switch (regs->eax) {
        case SYS_EXIT: {
            curr_proc->state = PROC_DEAD;
            curr_proc->exit_code = regs->ebx;

            process_cleanup(curr_proc);

            for (process_t *proc = proc_list; proc != NULL; proc = proc->next) {
                if (proc->state != PROC_BLOCKED || proc->block != PROC_PID)
                    continue;

                if (proc->target_pid != curr_proc->pid)
                    continue;

                proc->state = PROC_READY;
                proc->block = PROC_NONE;
                proc->target_pid = (uint32_t)-1;
            }

            schedule(regs);

            break;
        }

        case SYS_YIELD_HLT: {
            schedule(regs);

            __asm__ volatile ("sti; hlt");

            regs->eax = 0;

            break;
        }

        case SYS_PRINT: {
            const char *user_msg = (const char *)regs->ebx;

            if (!paging_validate_user_range(curr_proc->pd_phys, (uint32_t)user_msg, 1, 0)) {
                regs->eax = (uint32_t)-1;

                break;
            }

            char kbuf[256];
            uint32_t i = 0;

            while (i < sizeof(kbuf) - 1) {
                uint32_t addr = (uint32_t)user_msg + i;

                if ((addr & 0xfff) == 0 && !paging_validate_user_range(curr_proc->pd_phys, addr, 1, 0))
                    break;

                kbuf[i] = user_msg[i];

                if (kbuf[i] == '\0')
                    break;

                i++;
            }

            kbuf[i] = '\0';

            term_write(kbuf);

            regs->eax = 0;

            break;
        }

        case SYS_DATETIME: {
            struct tm *out = (struct tm *)regs->ebx;

            if (!paging_validate_user_range(curr_proc->pd_phys, (uint32_t)out, sizeof(struct tm), 1)) {
                printf("[W] syscalls.c: SYS_DATETIME rejected invalid user pointer 0x%x (pid %d)\n",
                    (uint32_t)out, curr_proc->pid);

                regs->eax = (uint32_t)-1;

                break;
            }

            rtc_time_t t = rtc_now();

            out->tm_sec  = t.seconds;
            out->tm_min  = t.minutes;
            out->tm_hour = t.hours;
            out->tm_mday = t.day;
            out->tm_mon  = t.month;
            out->tm_year = t.year;

            regs->eax = 0;

            break;
        }

        case SYS_SBRK: {
            int32_t delta = (int32_t)regs->ebx;
            uint32_t old_break = curr_proc->heap_break;
            uint32_t new_break = old_break + delta;

            if (new_break < curr_proc->heap_start || new_break > curr_proc->heap_max) {
                regs->eax = (uint32_t)-1;

                break;
            }

            uint32_t old_top = (old_break + 0xfff) & ~0xfff;
            uint32_t new_top = (new_break + 0xfff) & ~0xfff;

            if (new_top > old_top) {
                for (uint32_t va = old_top; va < new_top; va += PAGE_SIZE) {
                    uint32_t frame = pmm_alloc_frame();

                    if (!frame) {
                        regs->eax = (uint32_t)-1;
                        
                        break;
                    }

                    if (paging_map_page(curr_proc->pd_phys, va, frame, PAGE_PRESENT | PAGE_RW | PAGE_USER)) {
                        pmm_free_frame(frame);

                        regs->eax = (uint32_t)-1;

                        break;
                    }
                    
                    memset(phys_to_virt(frame), 0, PAGE_SIZE);
                }
            } else if (new_top < old_top) {
                for (uint32_t va = new_top; va < old_top; va += PAGE_SIZE) {
                    uint32_t paddr;

                    if (paging_walk(curr_proc->pd_phys, va, &paddr) == 0) {
                        paging_unmap_page(curr_proc->pd_phys, va);
                        pmm_free_frame(paddr & ~0xfff);
                    }
                }
            }

            curr_proc->heap_break = new_break;
            regs->eax = old_break;
            
            break;
        }

        case SYS_OPEN: {
            const char *path = (const char *)regs->ebx;
            int writable = (int)regs->ecx;

            if (!paging_validate_user_range(curr_proc->pd_phys, (uint32_t)path, 1, 0)) {
                regs->eax = (uint32_t)-1;

                break;
            }

            int fd_slot = alloc_fd(curr_proc);

            if (fd_slot < 0) {
                regs->eax = (uint32_t)-1;

                break;
            }

            int file_idx = file_open(path, writable);

            if (file_idx < 0) {
                regs->eax = (uint32_t)-1;

                break;
            }

            curr_proc->fds[fd_slot] = file_idx;
            regs->eax = fd_slot;

            break;
        }

        case SYS_CLOSE: {
            int fd = (int)regs->ebx;

            if (fd < 0 || fd >= MAX_FDS_PER_PROC || curr_proc->fds[fd] == -1) {
                regs->eax = (uint32_t)-1;

                break;
            }

            file_close(curr_proc->fds[fd]);

            curr_proc->fds[fd] = -1;
            regs->eax = 0;

            break;
        }

        case SYS_READ: {
            int fd = (int)regs->ebx;
            void *buf = (void *)regs->ecx;
            uint32_t len = regs->edx;

            if (fd < 0 || fd >= MAX_FDS_PER_PROC || curr_proc->fds[fd] == -1 || !paging_validate_user_range(curr_proc->pd_phys, (uint32_t)buf, len, 1)) {
                regs->eax = (uint32_t)-1;

                break;
            }

            regs->eax = (uint32_t)file_read(curr_proc->fds[fd], buf, len);

            break;
        }

        case SYS_WRITE: {
            int fd = (int)regs->ebx;
            const void *buf = (const void *)regs->ecx;
            uint32_t len = regs->edx;

            if (fd < 0 || fd >= MAX_FDS_PER_PROC || curr_proc->fds[fd] == -1 || !paging_validate_user_range(curr_proc->pd_phys, (uint32_t)buf, len, 0)) {
                regs->eax = (uint32_t)-1;

                break;
            }

            regs->eax = (uint32_t)file_write(curr_proc->fds[fd], buf, len);

            break;
        }

        case SYS_STAT: {
            char kpath[64];
            void *out = (void *)regs->ecx;

            if (copy_user_str((const char *)regs->ebx, kpath, sizeof(kpath))) {
                regs->eax = (uint32_t)-1;

                break;
            }

            if (!paging_validate_user_range(curr_proc->pd_phys, (uint32_t)out, sizeof(fat_stat_t), 1)) {
                regs->eax = (uint32_t)-1;

                break;
            }

            fat_stat_t st;

            if (fat_stat(kpath, &st)) {
                regs->eax = (uint32_t)-1;

                break;
            }

            memcpy(out, &st, sizeof(fat_stat_t));

            regs->eax = 0;

            break;
        }

        case SYS_UNLINK: {
            char kpath[64];

            if (copy_user_str((const char *)regs->ebx, kpath, (sizeof(kpath)))) {
                regs->eax = (uint32_t)-1;

                break;
            }

            regs->eax = fat_delete_file(kpath) ? (uint32_t)-1 : 0;

            break;
        }

        case SYS_MKDIR: {
            char kpath[64];

            if (copy_user_str((const char *)regs->ebx, kpath, sizeof(kpath))) {
                regs->eax = (uint32_t)-1;

                break;
            }

            regs->eax = fat_mkdir(kpath) ? (uint32_t)-1 : 0;

            break;
        }

        case SYS_RMDIR: {
            char kpath[64];

            if (copy_user_str((const char *)regs->ebx, kpath, sizeof(kpath))) {
                regs->eax = (uint32_t)-1;

                break;
            }

            regs->eax = fat_rmdir(kpath) ? (uint32_t)-1 : 0;

            break;
        }

        case SYS_OPENDIR: {
            char kpath[64];

            if (copy_user_str((const char *)regs->ebx, kpath, sizeof(kpath))) {
                regs->eax = (uint32_t)-1;

                break;
            }

            int slot = alloc_dirfd(curr_proc);

            if (slot < 0) {
                regs->eax = (uint32_t)-1;

                break;
            }

            uint16_t cluster;

            if (fat_opendir(kpath, &cluster)) {
                regs->eax = (uint32_t)-1;

                break;
            }

            curr_proc->dirs[slot].cluster = cluster;
            curr_proc->dirs[slot].index = 0;
            curr_proc->dirs[slot].in_use = 1;

            regs->eax = slot;

            break;
        }

        case SYS_READDIR: {
            int dirfd = (int)regs->ebx;
            fat_dirent_info_t *out = (fat_dirent_info_t *)regs->ecx;

            if (dirfd < 0 || dirfd >= MAX_DIRS_PER_PROC || !curr_proc->dirs[dirfd].in_use) {
                regs->eax = (uint32_t)-1;

                break;
            }

            if (!paging_validate_user_range(curr_proc->pd_phys, (uint32_t)out, sizeof(fat_dirent_info_t), 1)) {
                regs->eax = (uint32_t)-1;

                break;
            }

            dir_handle_t *dh = &curr_proc->dirs[dirfd];
            fat_dirent_info_t entry;
            int err = fat_readdir(dh->cluster, dh->index, &entry);

            if (err) {
                regs->eax = 0;

                break;
            }

            dh->index++;

            memcpy(out, &entry, sizeof(fat_dirent_info_t));

            regs->eax = 1;

            break;
        }

        case SYS_CLOSEDIR: {
            int dirfd = (int)regs->ebx;

            if (dirfd < 0 || dirfd >= MAX_DIRS_PER_PROC || !curr_proc->dirs[dirfd].in_use) {
                regs->eax = (uint32_t)-1;

                break;
            }

            curr_proc->dirs[dirfd].in_use = 0;

            regs->eax = 0;

            break;
        }

        case SYS_SPAWN: {
            char kpath[64];

            if (copy_user_str((const char *)regs->ebx, kpath, sizeof(kpath))) {
                regs->eax = (uint32_t)-1;

                break;
            }

            int user_argc = (int)regs->ecx;
            const char *const *user_argv = (const char *const *)regs->edx;

            if (user_argc < 0 || user_argc > MAX_SPAWN_ARGC) {
                regs->eax = (uint32_t)-1;

                break;
            }

            char *kargv[MAX_SPAWN_ARGC];
            char kargbuf[MAX_SPAWN_ARGC][MAX_SPAWN_ARGLEN];
            int kargc;

            if (user_argc == 0 || user_argv == NULL) {
                strncpy(kargbuf[0], kpath, MAX_SPAWN_ARGLEN - 1);

                kargbuf[0][MAX_SPAWN_ARGLEN - 1] = '\0';
                kargv[0] = kargbuf[0];
                kargc = 1;
            } else {
                if (!paging_validate_user_range(curr_proc->pd_phys, (uint32_t)user_argv, user_argc * sizeof(char *), 0)) {
                    regs->eax = (uint32_t)-1;

                    break;
                }

                kargc = user_argc;

                for (int i = 0; i < kargc; i++) {
                    const char *user_str = user_argv[i];

                    if (copy_user_str(user_str, kargbuf[i], MAX_SPAWN_ARGLEN)) {
                        regs->eax = (uint32_t)-1;

                        break;
                    }

                    kargv[i] = kargbuf[i];
                }
            }

            int err = 0;
            process_t *child = process_create(kpath, kargc, kargv, &err);

            if (!child) {
                regs->eax = (uint32_t)-1;

                break;
            }

            child->ppid = curr_proc->pid;

            set_proc_cwd(child, curr_proc->cwd);
            scheduler_add(child);

            regs->eax = child->pid;

            break;
        }

        case SYS_KILL: {
            process_t *proc = process_find(regs->ebx);

            if (!proc) {
                regs->eax = (uint32_t)-1;

                break;
            }

            proc->state = PROC_DEAD;

            for (process_t *other_proc = proc_list; other_proc != NULL; other_proc = other_proc->next) {
                if (other_proc->state != PROC_BLOCKED || other_proc->block != PROC_PID)
                    continue;

                if (other_proc->target_pid != proc->pid)
                    continue;

                other_proc->state = PROC_READY;
                other_proc->block = PROC_NONE;
                other_proc->target_pid = (uint32_t)-1;
            }

            if (proc == curr_proc)
                schedule(regs);
            
            regs->eax = 0;

            break;
        }

        case SYS_WAIT: {
            process_t *proc = process_find(regs->ebx);

            if (!proc) {
                regs->eax = (uint32_t)-1;

                break;
            }

            if (proc->state == PROC_DEAD) {
                regs->eax = (uint32_t)proc->exit_code;

                break;
            }

            curr_proc->state = PROC_BLOCKED;
            curr_proc->block = PROC_PID;
            curr_proc->target_pid = proc->pid;

            schedule(regs);

            proc = process_find(regs->ebx);

            int code = proc->exit_code;

            proc_unlink(proc);
            kfree((uint32_t)proc);

            regs->eax = (uint32_t)code;

            break;
        }

        case SYS_SLEEP: {
            uint32_t ms = regs->ebx;

            curr_proc->state = PROC_BLOCKED;
            curr_proc->block = PROC_SLEEP;
            curr_proc->resume_tick = pit_ticks() + ms;

            schedule(regs);

            regs->eax = 0;

            break;
        }

        case SYS_GETCHAR: {
            int c = kbd_getchar();

            if (c < 0) {
                curr_proc->state = PROC_BLOCKED;
                curr_proc->block = PROC_INPUT;

                schedule(regs);

                c = kbd_getchar();
            }

            regs->eax = (uint32_t)c;

            break;
        }

        case SYS_GETCHAR_NB: {
            regs->eax = (uint32_t)kbd_getchar();

            break;
        }

        case SYS_GETUPTIME: {
            regs->eax = pit_ticks();

            break;
        }

        case SYS_PS: {
            proc_info_t *out = (proc_info_t *)regs->ebx;
            uint32_t max = regs->ecx;
            uint32_t total = 0;

            for (process_t *proc = proc_list; proc != NULL; proc = proc->next)
                total++;
            
            if (out == NULL || max == 0) {
                regs->eax = total;

                break;
            }

            if (!paging_validate_user_range(curr_proc->pd_phys, (uint32_t)out, max * sizeof(proc_info_t), 1)) {
                regs->eax = (uint32_t)-1;

                break;
            }

            uint32_t i = 0;

            for (process_t *proc = proc_list; proc != NULL && i < max; proc = proc->next) {
                proc_info_t info;

                info.pid = proc->pid;
                info.ppid = proc->ppid;
                info.state = proc->state;

                strncpy(info.name, proc->name, MAX_PROC_NAME - 1);

                info.name[MAX_PROC_NAME - 1] = '\0';

                memcpy(&out[i], &info, sizeof(proc_info_t));

                i++;
            }

            regs->eax = i;

            break;
        }

        case SYS_CHDIR: {
            char kpath[MAX_BUF];

            if (copy_user_str((const char *)regs->ebx, kpath, sizeof(kpath))) {
                regs->eax = (uint32_t)-1;

                break;
            }

            fat_stat_t st;
            
            if (fat_stat(kpath, &st) != 0 && !(st.attr & 0x10)) {
                regs->eax = (uint32_t)-1;

                break;
            }

            set_proc_cwd(curr_proc, kpath);

            regs->eax = 0;

            break;
        }

        case SYS_GETCWD: {
            char *out = (char *)regs->ebx;
            uint32_t max = regs->ecx;

            if (!paging_validate_user_range(curr_proc->pd_phys, (uint32_t)out, max, 1)) {
                regs->eax = (uint32_t)-1;

                break;
            }

            uint32_t len = strlen(curr_proc->cwd);

            if (len >= max)
                len = max - 1;
            
            memcpy(out, curr_proc->cwd, len);

            out[len] = '\0';
            regs->eax = 0;

            break;
        }

        default: {
            printf("[W] syscalls.c: syscall_dispatch() -> unknown syscall: %d\n", regs->eax);

            regs->eax = (uint32_t)-1;
        }
    }
}