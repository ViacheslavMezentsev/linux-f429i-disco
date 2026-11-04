/* Bootlin armv7m uClibc 2024.05-1 exposes 64-bit time_t, but its
 * clock_gettime calls ARM syscall 263, which returns two 32-bit words.
 * Use the Linux 6.14 ARM EABI time64 layout at the kernel boundary.
 */
#include <errno.h>
#include <stdint.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

enum { arm_clock_gettime64 = 403, arm_pselect6_time64 = 413 };
struct kernel_timespec64 { int64_t sec, nsec; };

int __wrap_clock_gettime(clockid_t clock, struct timespec *out)
{
    struct kernel_timespec64 result;
    if (syscall(arm_clock_gettime64, clock, &result) < 0)
        return -1;
    out->tv_sec = result.sec;
    out->tv_nsec = result.nsec;
    return 0;
}

time_t __wrap_time(time_t *out)
{
    struct timespec result;
    if (__wrap_clock_gettime(CLOCK_REALTIME, &result) < 0)
        return (time_t)-1;
    if (out)
        *out = result.tv_sec;
    return result.tv_sec;
}

int __wrap_gettimeofday(struct timeval *out, void *zone)
{
    struct timespec result;
    (void)zone; /* Dropbear does not request legacy timezone information. */
    if (__wrap_clock_gettime(CLOCK_REALTIME, &result) < 0)
        return -1;
    out->tv_sec = result.tv_sec;
    out->tv_usec = result.tv_nsec / 1000;
    return 0;
}

int __wrap_select(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                  struct timeval *timeout)
{
    struct kernel_timespec64 ts;
    if (timeout) {
        if (timeout->tv_sec < 0 || timeout->tv_usec < 0 || timeout->tv_usec >= 1000000) {
            errno = EINVAL;
            return -1;
        }
        ts.sec = timeout->tv_sec;
        ts.nsec = timeout->tv_usec * 1000;
    }
    int result = syscall(arm_pselect6_time64, n, readfds, writefds, exceptfds,
                         timeout ? &ts : NULL, NULL);
    if (timeout && (result >= 0 || errno == EINTR)) {
        timeout->tv_sec = ts.sec;
        timeout->tv_usec = ts.nsec / 1000;
    }
    return result;
}

/* uClibc linuxthreads keeps callbacks in a userspace table shared across
 * vfork(). Dropbear resets SIGCHLD to SIG_DFL in the child before exec.
 * Register this one handler directly in the kernel for the single-threaded
 * parent so its callback survives the child's update to the libc table.
 */
extern int __libc_sigaction(int, const struct sigaction *, struct sigaction *);
extern int __real_sigaction(int, const struct sigaction *, struct sigaction *);

int __wrap_sigaction(int signum, const struct sigaction *act, struct sigaction *old)
{
    if (signum == SIGCHLD)
        return __libc_sigaction(signum, act, old);
    return __real_sigaction(signum, act, old);
}
