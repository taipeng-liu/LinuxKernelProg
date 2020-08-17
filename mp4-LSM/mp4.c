#define pr_fmt(fmt) "cs423_mp4: " fmt

#include <linux/lsm_hooks.h>
#include <linux/security.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/cred.h>
#include <linux/dcache.h>
#include <linux/binfmts.h>
#include "mp4_given.h"

#define MAX_BUF_LEN 4096
#define MAX_XATTR_LEN 30
#define DEFAULT_XATTR_VAL "read-write"
#define TEST "NO_OSID"
#define SID_NOTFOUND -1
#define MY_TEST_SID_(n) 20+n
#define MP4_NONTARGET_SID -7
/**
 * get_inode_sid - Get the inode mp4 security label id
 *
 * @inode: the input inode
 *
 * @return the inode's security id if found, -1 if not found
 *
 */
static int get_inode_sid(struct inode *inode)
{
	int ret, err;
	struct dentry *dentry = NULL;
	char xattr_val[MAX_XATTR_LEN];

	BUG_ON(!inode);
	
	//Check existence of getxattr
	if(!inode->i_op->getxattr) {
		//Non-target program
		goto notfound;
	}

	//Get dentry
	dentry = d_find_alias(inode);
	if (!dentry) {
		pr_err("get_inode_sid(): Get dentry failed\n");
		goto notfound;
	}

	//Get xattr value 
	err = inode->i_op->getxattr(dentry, XATTR_NAME_MP4, xattr_val, MAX_XATTR_LEN);
	if (err < 0) {
		//TODO handle different types of error
		//pr_err("Error %d on i_op->getxattr\n", err);
		goto notfound;
	}

	xattr_val[err] = '\0';
	
	ret = __cred_ctx_to_sid(xattr_val);

	if (dentry)
		dput(dentry);

	return ret;

notfound:
	if (dentry)
		dput(dentry);
	return SID_NOTFOUND;
}

/**
 * mp4_bprm_set_creds - Set the credentials for a new task
 *
 * @bprm: The linux binary preparation structure
 *
 * returns 0 on success.
 */
static int mp4_bprm_set_creds(struct linux_binprm *bprm)
{
	int sid;
	int ret = 0;
	char *path_buf = NULL, *inode_path = NULL;
	struct dentry *dentry = NULL;
	struct mp4_security *mp4_secp = bprm->cred->security;
	struct inode *inode = file_inode(bprm->file);
	
	//Only depend on initial program/script, not the interpreter
	if (bprm->cred_prepared)
		goto done;
	
	//Lookup path
	path_buf = kcalloc(MAX_BUF_LEN, sizeof(*path_buf), GFP_KERNEL);
	if (path_buf == NULL) {
		//ret = -EACCES;
		pr_err("get_inode_path1: kcalloc() failed\n");
		goto done;
	}

	dentry = d_find_alias(inode);
	if (dentry == NULL) {
		//ret = -EACCES;
		pr_err("get_inode_path1: Get dentry failed\n");
		goto done;
	}

	inode_path = dentry_path_raw(dentry, path_buf, MAX_BUF_LEN);
	if (inode_path == NULL) {
		//ret = -EACCES;
		pr_err("get_inode_path1: path lookup failed\n");
		goto done;
	}

	//Check security context
	if (mp4_secp == NULL) {
		pr_warn("bprm(): %s has no security context! Re-initializing...\n", inode_path);
		mp4_secp = kzalloc(sizeof(struct mp4_security), GFP_KERNEL);
		if (mp4_secp == NULL) {
			ret = -ENOMEM;
			goto done;
		}
		mp4_secp->mp4_flags = MP4_NO_ACCESS;
	}

	//Get sid from the inode
	sid = get_inode_sid(inode);

	//Set lable of the task to "target"
	if (sid == MP4_TARGET_SID) {
		pr_info("bprm(): %s is target\n", inode_path);
		mp4_secp->mp4_flags = MP4_TARGET_SID;
	} else {
		//pr_warn("bprm(): \n is non-target, mp4_flags := MP4_NO_ACCESS", inode_path);
		mp4_secp->mp4_flags = MP4_NONTARGET_SID;
	}

done:
	if (path_buf)
		kfree(path_buf);
	if (dentry)
		dput(dentry);
	return 0;
}

/**
 * mp4_cred_alloc_blank - Allocate a blank mp4 security label
 *
 * @cred: the new credentials
 * @gfp: the atomicity of the memory allocation
 *
 */
static int mp4_cred_alloc_blank(struct cred *cred, gfp_t gfp)
{
	struct mp4_security *mp4_secp;

	mp4_secp = kzalloc(sizeof(struct mp4_security), gfp);
	if (mp4_secp == NULL)
		return -ENOMEM;

	mp4_secp->mp4_flags = MP4_NO_ACCESS;
	cred->security = mp4_secp;

	return 0;
}


/**
 * mp4_cred_free - Free a created security label
 *
 * @cred: the credentials struct
 *
 */
static void mp4_cred_free(struct cred *cred)
{
	struct mp4_security *mp4_secp = cred->security;

	if (mp4_secp == NULL)
		return;

	kfree(mp4_secp);
	cred->security = NULL;
}

/**
 * mp4_cred_prepare - Prepare new credentials for modification
 *
 * @new: the new credentials
 * @old: the old credentials
 * @gfp: the atomicity of the memory allocation
 *
 */
static int mp4_cred_prepare(struct cred *new, const struct cred *old,
			    gfp_t gfp)
{
	const struct mp4_security *old_secp = old->security;
	struct mp4_security *cur_secp = current_security();
	struct mp4_security *new_secp;

	BUG_ON(!new);
	BUG_ON(new->security);
	BUG_ON(!old);

	if (old_secp == NULL)
	{
		if (cur_secp == NULL) {
			new_secp = kzalloc(sizeof(struct mp4_security), gfp);
			if (new_secp == NULL)
				return -ENOMEM;
			new_secp->mp4_flags = MP4_NO_ACCESS;
		} else {
			new_secp = kmemdup(cur_secp, sizeof(struct mp4_security), gfp);
			if (new_secp == NULL)
				return -ENOMEM;
		}

		new->security = new_secp;
		return 0;
	}

	new_secp = kmemdup(old_secp, sizeof(struct mp4_security), gfp);
	if (new_secp == NULL)
		return -ENOMEM;

	new->security = new_secp;
	return 0;
}

/**
 * mp4_inode_init_security - Set the security attribute of a newly created inode
 *
 * @inode: the newly created inode
 * @dir: the containing directory
 * @qstr: unused
 * @name: where to put the attribute name
 * @value: where to put the attribute value
 * @len: where to put the length of the attribute
 *
 * returns 0 if all goes well, -ENOMEM if no memory, -EOPNOTSUPP to skip
 *
 */
//TODO Please check this function if something gets wrong...
static int mp4_inode_init_security(struct inode *inode, struct inode *dir,
				   const struct qstr *qstr,
				   const char **name, void **value, size_t *len)
{
	const struct mp4_security *cur_mp4_secp = current_security();

	if (cur_mp4_secp == NULL || cur_mp4_secp->mp4_flags != MP4_TARGET_SID)
		return -EOPNOTSUPP;
	
	//TODO DEBUG: Do I need to find the path?????
	//pr_warn("A target program is creating an inode\n");
	
	if (name) {
		*name = kstrdup("mp4", GFP_KERNEL);
		if (*name == NULL)
			return -ENOMEM;
	}
	
	if (value && len) {
		*value = kstrdup("read-write", GFP_KERNEL);
		if (*value == NULL)
			return -ENOMEM;

		*len = strlen("read-write");
	}

	return 0;
}

/**
 * mp4_has_permission - Check if subject has permission to an object
 *
 * @ssid: the subject's security id
 * @osid: the object's security id
 * @mask: the operation mask
 *
 * returns 0 is access granter, -EACCES otherwise
 *
 */
static int mp4_has_permission(int ssid, int osid, int mask)
{
	if (ssid == MP4_TARGET_SID) {
		/* This is a target program */
		switch (osid) {
		/* Non-directory file */
		case MP4_NO_ACCESS:
			return -EACCES;
			break;
		case MP4_READ_OBJ:
			if (mask & (MAY_WRITE|MAY_APPEND|MAY_EXEC))
				return -EACCES;
			break;
		case MP4_READ_WRITE:
			if (mask & MAY_EXEC)
				return -EACCES;
			break;
		case MP4_WRITE_OBJ:
			if (mask & (MAY_READ|MAY_EXEC))
				return -EACCES;
			break;
		case MP4_EXEC_OBJ:
			if (mask & (MAY_WRITE|MAY_APPEND))
				return -EACCES;
			break;
		/* Directory */
		case MP4_READ_DIR:
			if (mask & (MAY_WRITE|MAY_APPEND))
				return -EACCES;
			break;
		case MP4_RW_DIR:
			break;
		default:
			break;
		}
	} else {
		/* This is not a target program */
		switch (osid) {
		/* Non-directory file */
		case MP4_NO_ACCESS:
			break;
		case MP4_READ_OBJ:
			if (mask & (MAY_WRITE|MAY_APPEND|MAY_EXEC))
				return -EACCES;
			break;
		case MP4_READ_WRITE:
			if (mask & (MAY_WRITE|MAY_APPEND|MAY_EXEC))
				return -EACCES;
			break;
		case MP4_WRITE_OBJ:
			if (mask & (MAY_WRITE|MAY_APPEND|MAY_EXEC))
				return -EACCES;
			break;
		case MP4_EXEC_OBJ:
			if (mask & (MAY_WRITE|MAY_APPEND))
				return -EACCES;
			break;
		/* Directory */
		case MP4_READ_DIR:
			break;
		case MP4_RW_DIR:
			break;
		default:
			break;
		}
	}

	return 0;
}

/**
 * mp4_inode_permission - Check permission for an inode being opened
 *
 * @inode: the inode in question
 * @mask: the access requested
 *
 * This is the important access check hook
 *
 * returns 0 if access is granted, -EACCES otherwise
 *
 */
static int mp4_inode_permission(struct inode *inode, int mask)
{
	int ret = 0;
	char *inode_path = NULL, *path_buf = NULL;
	struct dentry *dentry = NULL;
	const struct cred *cred = current_cred();
	int ssid, osid;
	struct mp4_security *mp4_secp = NULL;

	mask &= (MAY_READ|MAY_WRITE|MAY_EXEC|MAY_APPEND);
	//No permission to check
	if (mask == 0)
		goto done;
	
	//Limit rate of print
	//printk_ratelimit();

	//Lookup path
	path_buf = kcalloc(MAX_BUF_LEN, sizeof(*path_buf), GFP_KERNEL);
	if (path_buf == NULL) {
		//ret = -EACCES;
		pr_err("get_inode_path: kcalloc() failed\n");
		goto done;
	}

	dentry = d_find_alias(inode);
	if (dentry == NULL) {
		//ret = -EACCES;
		pr_err("get_inode_path: Get dentry failed\n");
		goto done;
	}

	inode_path = dentry_path_raw(dentry, path_buf, MAX_BUF_LEN);
	if (inode_path == NULL) {
		//ret = -EACCES;
		pr_err("get_inode_path: path lookup failed\n");
		goto done;
	}

	//Skip path
	if (mp4_should_skip_path(inode_path))
		goto done;

	//Get ssid
	mp4_secp = cred->security;
	if (mp4_secp == NULL) {
		//pr_info("permission(): No security context\n");
		ssid = SID_NOTFOUND;
	} else {
		ssid = mp4_secp->mp4_flags;
	}

	//Get osid
	osid = get_inode_sid(inode);

	//Permission control
	if (mp4_has_permission(ssid, osid, mask) != 0) {
		ret = -EACCES;
		pr_warn("Permission denied to %s, ssid=%d, osid=%d, mask=%d\n", inode_path, ssid, osid, mask);
		goto done;
	} else {
		//if (ssid == MP4_TARGET_SID) {
		//	pr_warn("Permission allowed to %s, ssid=%d, osid=%d, mask=%d\n", inode_path, ssid, osid, mask);
		//}
		ret = 0;
		goto done;
	}
	
done:
	if (path_buf != NULL)
		kfree(path_buf);
	if (dentry != NULL)
		dput(dentry);
	return ret;
}


/*
 * This is the list of hooks that we will using for our security module.
 */
static struct security_hook_list mp4_hooks[] = {
	/*
	 * inode function to assign a label and to check permission
	 */
	LSM_HOOK_INIT(inode_init_security, mp4_inode_init_security),
	LSM_HOOK_INIT(inode_permission, mp4_inode_permission),

	/*
	 * setting the credentials subjective security label when laucnhing a
	 * binary
	 */
	LSM_HOOK_INIT(bprm_set_creds, mp4_bprm_set_creds),

	/* credentials handling and preparation */
	LSM_HOOK_INIT(cred_alloc_blank, mp4_cred_alloc_blank),
	LSM_HOOK_INIT(cred_free, mp4_cred_free),
	LSM_HOOK_INIT(cred_prepare, mp4_cred_prepare)
};

static __init int mp4_init(void)
{
	/*
	 * check if mp4 lsm is enabled with boot parameters
	 */
	if (!security_module_enable("mp4"))
		return 0;

	pr_info("mp4 LSM initializing..Test for %s...", TEST);

	/*
	 * Register the mp4 hooks with lsm
	 */
	
	security_add_hooks(mp4_hooks, ARRAY_SIZE(mp4_hooks));

	return 0;
}

/*
 * early registration with the kernel
 */
security_initcall(mp4_init);
