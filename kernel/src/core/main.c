#include "minemu/boot.h"
#include "minemu/trap.h"
#include "minemu/trace.h"
#include "minemu/user_abi.h"
#include <stdbool.h>
#include <stdint.h>
#include "minemu/mmu.h"
#define MINEMU_IOCTL_ERROR (-1)
#define MINEMU_SYSCALL_IOCTL 1
#define MINEMU_IOCTL_UART_WRITE 0x00000102
#define MINEMU_IOCTL_MAX_TRANSFER 256
#define MINEMU_PTE_USER UINT32_C(4)

static bool user_buffer_valid(
    uint32_t address,
    uint32_t length,
    uint32_t required_flags){
    
    (void)required_flags;

    if(length == 0){
        return true;
    }
    if(length > UINT32_MAX - address){
        return false;
    }
    return false;
}

void minemu_kernel_main(const struct minemu_boot_info *boot_info) {
    if ((uintptr_t)boot_info != MINEMU_BOOT_INFO_VADDR ||
        boot_info->magic != MINEMU_BOOT_INFO_MAGIC ||
        boot_info->version != MINEMU_ABI_VERSION ||
        boot_info->size != sizeof(*boot_info) ||
        boot_info->system_rom_base != UINT32_C(0x08000000) ||
        boot_info->direct_map_vaddr != UINT32_C(0xc0000000) ||
        boot_info->direct_map_paddr != UINT32_C(0x40000000) ||
        boot_info->direct_map_size != UINT32_C(0x04000000)) {
        minemu_trace_event(UINT32_C(0xb007bad0));
        minemu_fail_stop();
    }
    minemu_trace_event(1);
    minemu_fail_stop();
}

struct minemu_trap_frame *minemu_svc_dispatch(
    struct minemu_trap_frame *frame){
    if (frame->exception_id != MINEMU_EXCEPTION_SVC){
        frame->r[0] = MINEMU_IOCTL_ERROR;
        return frame;
    }

    uint32_t mode = frame->spsr & 0x1F;

    if (mode != 0x10){
        frame->r[0] = MINEMU_IOCTL_ERROR;
        return frame;
    }

    if (frame->r[7] != MINEMU_SYSCALL_IOCTL){
        frame->r[0] = MINEMU_IOCTL_ERROR;
        return frame;
    }

    switch (frame->r[1]) {
        case MINEMU_IOCTL_UART_WRITE:
            if (frame->r[0] != MINEMU_FD_UART_OUT){
                frame->r[0] = MINEMU_IOCTL_ERROR;
                return frame;
            }
            if (frame->r[3] > MINEMU_IOCTL_MAX_TRANSFER){
                frame->r[0] = MINEMU_IOCTL_ERROR;
                return frame;
            }
            if (frame->r[3] > 0 && frame->r[2] == 0x0){
                frame->r[0] = MINEMU_IOCTL_ERROR;
                return frame;
            }
            if (!user_buffer_valid(frame->r[2], frame->r[3], MINEMU_PTE_READABLE)) {
                frame->r[0] = MINEMU_IOCTL_ERROR;
                return frame;
            }
            break;
        default:
            frame->r[0] = MINEMU_IOCTL_ERROR;
            return frame;
    }
    return frame;
    }