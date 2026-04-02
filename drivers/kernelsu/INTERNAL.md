# Quirks / Adaptations

## hooking
- prefer syscalls always
- syscall table hooking is implemented but only for !CFI
- on legacy tehres no kprobes/kretprobes and syscall tracepoint guarantees
- theres no guarantee for kallsyms even!
- partial kp/rp support on boot-time hooks

## sucompat
- tweaked for downstream
- copy_from_user instead of strncpy_from_user
- a lil bit faster, no looped null term check

## task_fix_setuid LSM
- upstream was on this before
- for seccomp disabling and umount feature
#### we don't have seccomp filter caching
- we just disable seccomp on setuid LSM
- we also reuse this seccomp status as sucompat gate

## pkg_observer is on inode_rename LSM
- upstream was on this before
- this is faster, we filter uid
- we dont watch a full folder for shit
#### throne_tracker
- first run is synchronous by default due to FDE/FBEv1 (some)
- kthreaded on successive runs
- lock contention/double locking and race conditions are handled

## security_bprm_check LSM
#### lockless init filename and current->mm->argX grabbing
- this is one step late, but for what we do, this is fine
- https://stackoverflow.com/questions/65881204/get-argv-from-bprm-check-security-in-linux-kernel-is-the-documentation-wrong
#### init session keyring grab 
- we watch init process here too
#### lockless current->mm->argX grabbing for sulog
- yeah we can do this on execve but this is faster than looped copy_from_user
- we already have the hook, we reuse it

## security_file_permission LSM
- we use this to avoid hooking sys_read for manual hooks
- after all we just need file pointer
- however if theres syscall table hook or kprobes_ksud, we hook it on there instead

## triple pointer hacks
#### on envp pullouts for adb root
- on execveat (kernel) hook, we pull this on envp since
- struct user_arg_ptr envp = { .ptr.native = __envp };
- __envp is const char __user *const __user * envp
- so this becomes void * const char __user *const __user * envp
- this is also used on the execve hook
#### on toolkit's uname hax
- since we pass arg as reference of arg on sys_reboot
- this is actually void * const char __user * const char __user *

## theres little endian hacks on the codebase
- unused MSB reuse for tiny_sulog
- long to int dereferences

## compat handling
- always redefine/override if possible
- avoid heavy metaprogramming on macros
- kernel_compat.h for small functions
- kernel_compat.c for big functions marked __weak and tagged with extern on callee site

## build system
- unity build, single unit
- can cause heavy inlining (higher stack overflow risk)
- ensure heavy inlining control (inline, noinline attributes)
- stack safety is disabled
- redefine str/mem fn's to builtins if !FORTIFY_SOURCE
