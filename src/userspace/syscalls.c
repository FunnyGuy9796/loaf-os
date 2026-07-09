#include "syscalls.h"
#include "../libc/include/syscall.h"
#include "../vga.h"
#include "../misc/mem.h"
#include "../misc/printf.h"
#include "../multi/process.h"
#include "../devices/pit.h"
#include "../devices/rtc.h"
#include "../mm/pmm.h"
#include "../mm/paging.h"

static int alloc_fd(process_t *proc) {
    for (int i = 0; i < MAX_FDS_PER_PROC; i++)
        if (proc->fds[i] == -1)
            return i;

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

            process_t *proc = proc_list;

            do {
                if (proc->target_pid == curr_proc->pid && proc->state == PROC_BLOCKED) {
                    proc->state = PROC_READY;
                    proc->target_pid = -1;
                }

                proc = proc->next;
            } while (proc != NULL);

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

        case SYS_SPAWN: {
            char kpath[64];

            if (copy_user_str((const char *)regs->ebx, kpath, sizeof(kpath))) {
                regs->eax = (uint32_t)-1;

                break;
            }

            int err = 0;
            process_t *child = process_create(kpath, &err);

            if (!child) {
                regs->eax = (uint32_t)-1;

                break;
            }

            child->ppid = curr_proc->pid;

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

            process_t *other_proc = proc_list;

            do {
                if (other_proc->target_pid == proc->pid && other_proc->state == PROC_BLOCKED) {
                    other_proc->state = PROC_READY;
                    other_proc->target_pid = -1;
                }

                other_proc = other_proc->next;
            } while (other_proc != NULL);

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
            curr_proc->target_pid = proc->pid;

            schedule(regs);

            regs->eax = (uint32_t)proc->exit_code;

            break;
        }

        case SYS_SLEEP: {
            uint32_t ms = regs->ebx;

            curr_proc->state = PROC_BLOCKED;
            curr_proc->resume_tick = pit_ticks() + ms;

            schedule(regs);

            regs->eax = 0;

            break;
        }

        default: {
            printf("[W] syscalls.c: syscall_dispatch() -> unknown syscall: %d\n", regs->eax);

            regs->eax = (uint32_t)-1;
        }
    }
}