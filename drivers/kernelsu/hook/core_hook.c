#ifdef CONFIG_KSU_LSM_SECURITY_HOOKS
#define LSM_HANDLER_TYPE static int
#else
#define LSM_HANDLER_TYPE int
#endif

LSM_HANDLER_TYPE ksu_handle_rename(struct dentry *old_dentry, struct dentry *new_dentry)
{
	if (!current->mm) {
		// skip kernel threads
		return 0;
	}

	kuid_t current_uid = current_uid();
	if (ksu_get_uid_t(current_uid) != 1000) {
		// skip non system uid
		return 0;
	}

	if (!old_dentry || !new_dentry) {
		return 0;
	}

	// /data/system/packages.list.tmp -> /data/system/packages.list
	if (strcmp(new_dentry->d_iname, "packages.list")) {
		return 0;
	}

	char path[128];
	char *buf = dentry_path_raw(new_dentry, path, sizeof(path));
	if (IS_ERR(buf)) {
		pr_err("dentry_path_raw failed.\n");
		return 0;
	}

	if (!strstr(buf, "/system/packages.list")) {
		return 0;
	}
	pr_info("renameat: %s -> %s, new path: %s\n", old_dentry->d_iname,
		new_dentry->d_iname, buf);

	track_throne(false);

	return 0;
}

LSM_HANDLER_TYPE ksu_handle_setuid(struct cred *new, const struct cred *old)
{
	if (!new || !old) {
		return 0;
	}

	uid_t new_uid = ksu_get_uid_t(new->uid);
	uid_t old_uid = ksu_get_uid_t(old->uid);

	// old process is not root, ignore it.
	if (0 != old_uid)
		return 0;

	// we dont have those new fancy things upstream has
	// lets just do original thing where we disable seccomp
	if (likely(ksu_is_manager_appid_valid()) && unlikely(ksu_get_manager_appid() == new_uid % PER_USER_RANGE)) {
		disable_seccomp();
		pr_info("install fd for: %d\n", new_uid);
		ksu_install_fd(); // install fd for ksu manager
	}

	if (unlikely(ksu_is_allow_uid_for_current(new_uid))) {
		disable_seccomp();
		return 0;
	}

	return ksu_handle_umount(new, old);
}

LSM_HANDLER_TYPE ksu_bprm_check(struct linux_binprm *bprm)
{
#ifdef CONFIG_KSU_FEATURE_SULOG
	if (unlikely(!current->seccomp.mode))
		ksu_sulog_emit_bprm((const char *)bprm->filename);
#endif

	if (likely(!ksu_execveat_hook))
		return 0;

	ksu_grab_init_session_keyring((const char *)bprm->filename);

	ksu_handle_pre_ksud((const char *)bprm->filename);

	return 0;
}

LSM_HANDLER_TYPE ksu_file_permission(struct file *file, int mask)
{
	if (likely(!ksu_vfs_read_hook))
		return 0;

	ksu_install_rc_hook(file);

	return 0;
}

#ifdef CONFIG_KSU_LSM_SECURITY_HOOKS
static int ksu_inode_rename(struct inode *old_inode, struct dentry *old_dentry,
			    struct inode *new_inode, struct dentry *new_dentry)
{
	return ksu_handle_rename(old_dentry, new_dentry);
}

static int ksu_task_fix_setuid(struct cred *new, const struct cred *old,
			       int flags)
{
	return ksu_handle_setuid(new, old);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0)
static struct security_hook_list ksu_hooks[] = {
	LSM_HOOK_INIT(inode_rename, ksu_inode_rename),
	LSM_HOOK_INIT(task_fix_setuid, ksu_task_fix_setuid),
	LSM_HOOK_INIT(bprm_check_security, ksu_bprm_check),
	LSM_HOOK_INIT(file_permission, ksu_file_permission),
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
static void ksu_lsm_hook_init(void)
{
	security_add_hooks(ksu_hooks, ARRAY_SIZE(ksu_hooks), "ksu");
}

#else
static void ksu_lsm_hook_init(void)
{
	security_add_hooks(ksu_hooks, ARRAY_SIZE(ksu_hooks));
}
#endif //  < 4.11

#else // 4.2

// selinux_ops (LSM), security_operations struct tampering for ultra legacy

extern struct security_operations selinux_ops;

static int (*orig_inode_rename) (struct inode *old_dir, struct dentry *old_dentry,
			     struct inode *new_dir, struct dentry *new_dentry) = NULL;
static int hook_inode_rename(struct inode *old_inode, struct dentry *old_dentry,
			    struct inode *new_inode, struct dentry *new_dentry)
{
	ksu_inode_rename(old_inode, old_dentry, new_inode, new_dentry);
	return orig_inode_rename(old_inode, old_dentry, new_inode, new_dentry);
}

static int (*orig_task_fix_setuid) (struct cred *new, const struct cred *old, int flags) = NULL;
static int hook_task_fix_setuid(struct cred *new, const struct cred *old, int flags)
{
	ksu_task_fix_setuid(new, old, flags);
	return orig_task_fix_setuid(new, old, flags);
}

static int (*orig_bprm_check_security)(struct linux_binprm *bprm) = NULL;
static int hook_bprm_check_security(struct linux_binprm *bprm)
{
	ksu_bprm_check(bprm);
	return orig_bprm_check_security(bprm);
}

static int (*orig_file_permission) (struct file *file, int mask) = NULL;
static int hook_file_permission(struct file *file, int mask)
{

	ksu_file_permission(file, mask);
	return orig_file_permission(file, mask);
}

static void ksu_lsm_hook_restore(void)
{
	struct security_operations *ops = (struct security_operations *)&selinux_ops;

	if (!ops)
		return;

	if (!!strcmp((char *)ops, "selinux"))
		return;

	// TODO: maybe hunt for this in memory instead of exporting
	// this is the first member of the struct so it points to the struct
	pr_info("%s: selinux_ops: 0x%lx .name = %s\n", __func__, (long)ops, (const char *)ops );

	preempt_disable();
	local_irq_disable();

#if 0 // we need this now for sulog
	if (orig_bprm_check_security) {
		pr_info("%s: restoring: 0x%lx to 0x%lx\n", __func__, (long)ops->bprm_check_security, (long)orig_bprm_check_security);
		ops->bprm_check_security = orig_bprm_check_security;
	}
#endif

	if (orig_file_permission) {
		pr_info("%s: restoring: 0x%lx to 0x%lx\n", __func__, (long)ops->file_permission, (long)orig_file_permission);
		ops->file_permission = orig_file_permission;
	}

	smp_mb();

	local_irq_enable();
	preempt_enable();
	
	return;
}

static int execveat_hook_wait_fn(void *data)
{
loop_start:

	msleep(1000);

	if ((volatile bool)ksu_execveat_hook)
		goto loop_start;

	ksu_lsm_hook_restore();

	return 0;
}

static void execveat_hook_wait_thread()
{
	kthread_run(execveat_hook_wait_fn, NULL, "unhook");
}

static void ksu_lsm_hook_init(void)
{
	struct security_operations *ops = (struct security_operations *)&selinux_ops;

	if (!ops)
		return;

	if (!!strcmp((char *)ops, "selinux"))
		return;

	// TODO: maybe hunt for this in memory instead of exporting
	// this is the first member of the struct so it points to the struct
	pr_info("%s: selinux_ops: 0x%lx .name = %s\n", __func__, (long)ops, (const char *)ops );

	preempt_disable();
	local_irq_disable();

	orig_inode_rename = ops->inode_rename;
	ops->inode_rename = hook_inode_rename;

	orig_task_fix_setuid = ops->task_fix_setuid;
	ops->task_fix_setuid = hook_task_fix_setuid;

	orig_bprm_check_security = ops->bprm_check_security;
	ops->bprm_check_security = hook_bprm_check_security;

	orig_file_permission = ops->file_permission;
	ops->file_permission = hook_file_permission;

	smp_mb();

	local_irq_enable();
	preempt_enable();
	
	execveat_hook_wait_thread();
	return;
}

#endif // < 4.2

#else
void __init ksu_lsm_hook_init(void) { } // nothing, no-op
#endif // CONFIG_KSU_LSM_SECURITY_HOOKS

void __init ksu_core_init(void)
{
	ksu_lsm_hook_init();
}
