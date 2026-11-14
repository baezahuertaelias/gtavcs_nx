/* Default stack for threads created without an explicit size (std::thread, pthread_create
 * with default attributes). libnx's __syscall_thread_create picks 128 KiB for those; the
 * runtime's game thread runs the recompiled EE code and its host helpers on such a
 * std::thread, which on the desktop reference gets 8 MiB (Linux) or 1 MiB (Windows).
 * Linked with -Wl,--wrap=__syscall_thread_create so every default-sized thread in the
 * process gets the Linux reference size; explicit sizes pass through unchanged. */
#include <stdatomic.h>
#include <stddef.h>

#define GTAVCS_DEFAULT_THREAD_STACK (8u * 1024u * 1024u)

struct __pthread_t;
int __real___syscall_thread_create(struct __pthread_t **thread, void *(*func)(void *), void *arg,
    void *stack_addr, size_t stack_size);

static atomic_uint created_threads;

int __wrap___syscall_thread_create(struct __pthread_t **thread, void *(*func)(void *), void *arg,
    void *stack_addr, size_t stack_size)
{
    if (!stack_addr && !stack_size) stack_size = GTAVCS_DEFAULT_THREAD_STACK;
    const int result = __real___syscall_thread_create(thread, func, arg, stack_addr, stack_size);
    if (!result) atomic_fetch_add(&created_threads, 1);
    return result;
}

unsigned gtavcs_threads_created(void) { return atomic_load(&created_threads); }
size_t gtavcs_default_thread_stack(void) { return GTAVCS_DEFAULT_THREAD_STACK; }
