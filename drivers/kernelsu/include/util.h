#ifndef __KSU_H_UTIL
#define __KSU_H_UTIL

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
#define ksu_close_fd close_fd
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0) && LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
#define ksu_close_fd(fd) __close_fd(current->files, fd)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0)
#define ksu_close_fd sys_close
#endif

#endif // __KSU_H_UTIL
