/*
 * VFSX - External VFS Bridge
 * This transparent VFS module sends VFS operations over a Unix domain
 * socket for external handling.
 *
 * Copyright (C) 2004 Steven R. Farley.  All rights reserved.
 */

#include "includes.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

#define VFSX_MSG_OUT_SIZE 512
#define VFSX_MSG_IN_SIZE 3
#define VFSX_FAIL_ERROR -1
#define VFSX_FAIL_AUTHORIZATION -2
#define VFSX_SUCCESS_TRANSPARENT 0
#define VFSX_SOCKET_FILE "/tmp/vfsx-socket"
#define VFSX_LOG_FILE "/tmp/vfsx.log"

 /* VFSX communication functions */

static void vfsx_write_file(const char *str)
{
	int fd;

	fd = open(VFSX_LOG_FILE, O_RDWR | O_APPEND);
	if (fd != -1) {
		write(fd, str, strlen(str));
		write(fd, "\n", 1);
		close(fd);
	}
	else {
		syslog(LOG_NOTICE, "vfsx_write_file can't write");
	}
}

static int vfsx_write_socket(const char *str, int close_socket)
{
	static int connected = 0;
	static int sd = -1;
	static int count = 0;
	char out[VFSX_MSG_OUT_SIZE];
	char in[VFSX_MSG_IN_SIZE];
	int ret;
	struct sockaddr_un sa;
	int result = VFSX_FAIL_ERROR;

	if (!connected) {
		sd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (sd != -1) {
			strncpy(sa.sun_path, VFSX_SOCKET_FILE, strlen(VFSX_SOCKET_FILE) + 1);
			sa.sun_family = AF_UNIX;
			ret = connect(sd, (struct sockaddr *) &sa, sizeof(sa));
			if (ret != -1) {
				syslog(LOG_NOTICE, "vfsx_write_socket connect succeeded");
				connected = 1;
			}
			else {
				syslog(LOG_NOTICE, "vfsx_write_socket connect failed");
				close(sd);
			}
		}
		else {
			syslog(LOG_NOTICE, "vfsx_write_socket open failed");
		}
	}

	if (connected) {
		memset(out, 0, VFSX_MSG_OUT_SIZE);
		strncpy(out, str, strlen(str) + 1);
		ret = write(sd, out, VFSX_MSG_OUT_SIZE);
		if (ret != -1) {
			memset(in, 0, VFSX_MSG_IN_SIZE);
			ret = read(sd, in, VFSX_MSG_IN_SIZE);
			if (ret != -1) {
				result = atoi(in);
				syslog(LOG_NOTICE, "vfsx_write_socket (%d) received '%s'", count++, in);
				if (close_socket) {
					syslog(LOG_NOTICE, "vfsx_write_socket closing normally");
					close(sd);
					connected = 0;
				}
			}
			else {
				syslog(LOG_NOTICE, "vfsx_write_socket read failed");
				close(sd);
				connected = 0;
			}
		}
		else {
			syslog(LOG_NOTICE, "vfsx_write_socket write failed");
			close(sd);
			connected = 0;
		}
	}

	if (result == VFSX_FAIL_ERROR) {
		// TODO: Correct error code?
		errno = EIO;
	}
	else if (result == VFSX_FAIL_AUTHORIZATION) {
		errno = EPERM;
	}
	return result;
}

static int vfsx_execute(const char *buf, int count)
{
	int close_sock = 0;

	// buf = "user:operation:origpath:arg1,arg2,arg3"

	if (strncmp(buf, "disconnect", 10) == 0) {
		close_sock = 1;
	}

	if (count > 0) {
		//vfsx_write_file(buf);
		return vfsx_write_socket(buf, close_sock);
	}
	else {
		return VFSX_FAIL_ERROR;
	}
}

/* VFS handler functions */

static int vfsx_connect(vfs_handle_struct *handle, connection_struct *conn, const char *svc, const char *user)
{
	int result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "connect:%s:%s", conn->user, conn->origpath);
	if (vfsx_execute(buf, count) == VFSX_SUCCESS_TRANSPARENT) {
		result = SMB_VFS_NEXT_CONNECT(handle, conn, svc, user);
	}
	return result;
}

static void vfsx_disconnect(vfs_handle_struct *handle, connection_struct *conn)
{
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "disconnect:%s:%s", conn->user, conn->origpath);
	if (vfsx_execute(buf, count) == VFSX_SUCCESS_TRANSPARENT) {
		SMB_VFS_NEXT_DISCONNECT(handle, conn);
	}
	return;
}

static DIR *vfsx_opendir(vfs_handle_struct *handle, connection_struct *conn, const char *fname)
{
	// TODO: Is this the correct error value?
	DIR *result = NULL;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "opendir:%s:%s:%s", conn->user, conn->origpath, fname);
	if (vfsx_execute(buf, count) == VFSX_SUCCESS_TRANSPARENT) {
		result = SMB_VFS_NEXT_OPENDIR(handle, conn, fname);
	}
	return result;
}

static int vfsx_mkdir(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode)
{
	int result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "mkdir:%s:%s:%s,%d", conn->user, conn->origpath, path, mode);
	if (vfsx_execute(buf, count) == VFSX_SUCCESS_TRANSPARENT) {
		result = SMB_VFS_NEXT_MKDIR(handle, conn, path, mode);
	}
	return result;
}

static int vfsx_rmdir(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
	int result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "rmdir:%s:%s:%s", conn->user, conn->origpath, path);
	if (vfsx_execute(buf, count) == VFSX_SUCCESS_TRANSPARENT) {
		result = SMB_VFS_NEXT_RMDIR(handle, conn, path);
	}
	return result;
}

static int vfsx_open(vfs_handle_struct *handle, connection_struct *conn, const char *fname, int flags, mode_t mode)
{
	int result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "open:%s:%s:%s,%d,%d", conn->user, conn->origpath, fname, flags, mode);
	if (vfsx_execute(buf, count) == VFSX_SUCCESS_TRANSPARENT) {
		result = SMB_VFS_NEXT_OPEN(handle, conn, fname, flags, mode);
	}
	return result;
}

static int vfsx_close(vfs_handle_struct *handle, files_struct *fsp, int fd)
{
	int result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "close:%s:%s:%s", fsp->conn->user, fsp->conn->origpath, fsp->fsp_name);
	if (vfsx_execute(buf, count) == VFSX_SUCCESS_TRANSPARENT) {
		result = SMB_VFS_NEXT_CLOSE(handle, fsp, fd);
	}
	return result;
}

static ssize_t vfsx_read(vfs_handle_struct *handle, files_struct *fsp, int fd, void *data, size_t n)
{
	ssize_t result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "read:%s:%s:%s", fsp->conn->user, fsp->conn->origpath, fsp->fsp_name);
	if (vfsx_execute(buf, count) == VFSX_SUCCESS_TRANSPARENT) {
		result = SMB_VFS_NEXT_READ(handle, fsp, fd, data, n);
	}
	return result;
}

static ssize_t vfsx_write(vfs_handle_struct *handle, files_struct *fsp, int fd, const void *data, size_t n)
{
	ssize_t result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "write:%s:%s:%s", fsp->conn->user, fsp->conn->origpath, fsp->fsp_name);
	if (vfsx_execute(buf, count) == VFSX_SUCCESS_TRANSPARENT) {
		result = SMB_VFS_NEXT_WRITE(handle, fsp, fd, data, n);
	}
	return result;
}

static ssize_t vfsx_pread(vfs_handle_struct *handle, files_struct *fsp, int fd, void *data, size_t n, SMB_OFF_T offset)
{
	ssize_t result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "pread:%s:%s:%s", fsp->conn->user, fsp->conn->origpath, fsp->fsp_name);
	if (vfsx_execute(buf, count) == VFSX_SUCCESS_TRANSPARENT) {
		result = SMB_VFS_NEXT_PREAD(handle, fsp, fd, data, n, offset);
	}
	return result;
}

static ssize_t vfsx_pwrite(vfs_handle_struct *handle, files_struct *fsp, int fd, const void *data, size_t n, SMB_OFF_T offset)
{
	ssize_t result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "pwrite:%s:%s:%s", fsp->conn->user, fsp->conn->origpath, fsp->fsp_name);
	if (vfsx_execute(buf, count) == VFSX_SUCCESS_TRANSPARENT) {
		result = SMB_VFS_NEXT_PWRITE(handle, fsp, fd, data, n, offset);
	}
	return result;
}

static SMB_OFF_T vfsx_lseek(vfs_handle_struct *handle, files_struct *fsp, int filedes, SMB_OFF_T offset, int whence)
{
	SMB_OFF_T result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "lseek:%s:%s:%s", fsp->conn->user, fsp->conn->origpath, fsp->fsp_name);
	if (vfsx_execute(buf, count) == VFSX_SUCCESS_TRANSPARENT) {
		result = SMB_VFS_NEXT_LSEEK(handle, fsp, filedes, offset, whence);
	}
	return result;
}

static int vfsx_rename(vfs_handle_struct *handle, connection_struct *conn, const char *old, const char *new)
{
	int result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "rename:%s:%s:%s,%s", conn->user, conn->origpath, old, new);
	if (vfsx_execute(buf, count) == VFSX_SUCCESS_TRANSPARENT) {
		result = SMB_VFS_NEXT_RENAME(handle, conn, old, new);
	}
	return result;
}

static int vfsx_unlink(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
	int result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "unlink:%s:%s:%s", conn->user, conn->origpath, path);
	if (vfsx_execute(buf, count) == VFSX_SUCCESS_TRANSPARENT) {
		result = SMB_VFS_NEXT_UNLINK(handle, conn, path);
	}
	return result;
}

/*
static int vfsx_chmod(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode)
{
	int result;

	result = SMB_VFS_NEXT_CHMOD(handle, conn, path, mode);
	return result;
}

static int vfsx_chmod_acl(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode)
{
	int result;

	result = SMB_VFS_NEXT_CHMOD_ACL(handle, conn, path, mode);
	return result;
}

static int vfsx_fchmod(vfs_handle_struct *handle, files_struct *fsp, int fd, mode_t mode)
{
	int result;

	result = SMB_VFS_NEXT_FCHMOD(handle, fsp, fd, mode);
	return result;
}

static int vfsx_fchmod_acl(vfs_handle_struct *handle, files_struct *fsp, int fd, mode_t mode)
{
	int result;

	result = SMB_VFS_NEXT_FCHMOD_ACL(handle, fsp, fd, mode);
	return result;
}
*/

/* VFS operations */

static vfs_op_tuple vfsx_op_tuples[] = {

	/* Disk operations */

	{SMB_VFS_OP(vfsx_connect), 		SMB_VFS_OP_CONNECT, 	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_disconnect), 	SMB_VFS_OP_DISCONNECT, 	SMB_VFS_LAYER_TRANSPARENT},

	/* Directory operations */

	{SMB_VFS_OP(vfsx_opendir), 	SMB_VFS_OP_OPENDIR, 	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_mkdir), 	SMB_VFS_OP_MKDIR, 		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_rmdir), 	SMB_VFS_OP_RMDIR, 		SMB_VFS_LAYER_TRANSPARENT},

	/* File operations */

	{SMB_VFS_OP(vfsx_open), 		SMB_VFS_OP_OPEN, 		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_close), 		SMB_VFS_OP_CLOSE, 		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_read),			SMB_VFS_OP_READ,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_write),		SMB_VFS_OP_WRITE,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_pread),		SMB_VFS_OP_PREAD,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_pwrite),		SMB_VFS_OP_PWRITE,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_lseek),		SMB_VFS_OP_LSEEK,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_rename), 		SMB_VFS_OP_RENAME, 		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_unlink), 		SMB_VFS_OP_UNLINK, 		SMB_VFS_LAYER_TRANSPARENT},
/*
	{SMB_VFS_OP(vfsx_chmod), 		SMB_VFS_OP_CHMOD, 		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_fchmod), 		SMB_VFS_OP_FCHMOD, 		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_chmod_acl), 	SMB_VFS_OP_CHMOD_ACL, 	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_fchmod_acl), 	SMB_VFS_OP_FCHMOD_ACL, 	SMB_VFS_LAYER_TRANSPARENT},
*/

	/* Finish VFS operations definition */

	{SMB_VFS_OP(NULL), 				SMB_VFS_OP_NOOP, 		SMB_VFS_LAYER_NOOP}
};

/*
	// This is the complete list of implementable VFS operations, from the Samba
	// distribution file "examples/VFS/skel_transparent.c".

	// Disk operations

	{SMB_VFS_OP(skel_connect),				SMB_VFS_OP_CONNECT, 				SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_disconnect),			SMB_VFS_OP_DISCONNECT,				SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_disk_free),			SMB_VFS_OP_DISK_FREE,				SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_get_quota),			SMB_VFS_OP_GET_QUOTA,				SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_set_quota),			SMB_VFS_OP_SET_QUOTA,				SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_get_shadow_copy_data),	SMB_VFS_OP_GET_SHADOW_COPY_DATA,	SMB_VFS_LAYER_TRANSPARENT},

	// Directory operations

	{SMB_VFS_OP(skel_opendir),			SMB_VFS_OP_OPENDIR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_readdir),			SMB_VFS_OP_READDIR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_mkdir),			SMB_VFS_OP_MKDIR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_rmdir),			SMB_VFS_OP_RMDIR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_closedir),			SMB_VFS_OP_CLOSEDIR,	SMB_VFS_LAYER_TRANSPARENT},

	// File operations

	{SMB_VFS_OP(skel_open),				SMB_VFS_OP_OPEN,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_close),			SMB_VFS_OP_CLOSE,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_read),				SMB_VFS_OP_READ,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_write),			SMB_VFS_OP_WRITE,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_lseek),			SMB_VFS_OP_LSEEK,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_rename),			SMB_VFS_OP_RENAME,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_fsync),			SMB_VFS_OP_FSYNC,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_stat),				SMB_VFS_OP_STAT,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_fstat),			SMB_VFS_OP_FSTAT,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_lstat),			SMB_VFS_OP_LSTAT,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_unlink),			SMB_VFS_OP_UNLINK,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_chmod),			SMB_VFS_OP_CHMOD,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_fchmod),			SMB_VFS_OP_FCHMOD,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_chown),			SMB_VFS_OP_CHOWN,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_fchown),			SMB_VFS_OP_FCHOWN,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_chdir),			SMB_VFS_OP_CHDIR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_getwd),			SMB_VFS_OP_GETWD,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_utime),			SMB_VFS_OP_UTIME,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_ftruncate),		SMB_VFS_OP_FTRUNCATE,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_lock),				SMB_VFS_OP_LOCK,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_symlink),			SMB_VFS_OP_SYMLINK,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_readlink),			SMB_VFS_OP_READLINK,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_link),				SMB_VFS_OP_LINK,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_mknod),			SMB_VFS_OP_MKNOD,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_realpath),			SMB_VFS_OP_REALPATH,	SMB_VFS_LAYER_TRANSPARENT},

	// NT File ACL operations

	{SMB_VFS_OP(skel_fget_nt_acl),	SMB_VFS_OP_FGET_NT_ACL,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_get_nt_acl),	SMB_VFS_OP_GET_NT_ACL,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_fset_nt_acl),	SMB_VFS_OP_FSET_NT_ACL,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_set_nt_acl),	SMB_VFS_OP_SET_NT_ACL,	SMB_VFS_LAYER_TRANSPARENT},

	// POSIX ACL operations

	{SMB_VFS_OP(skel_chmod_acl),	SMB_VFS_OP_CHMOD_ACL,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_fchmod_acl),	SMB_VFS_OP_FCHMOD_ACL,	SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(skel_sys_acl_get_entry),		SMB_VFS_OP_SYS_ACL_GET_ENTRY,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_get_tag_type),		SMB_VFS_OP_SYS_ACL_GET_TAG_TYPE,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_get_permset),		SMB_VFS_OP_SYS_ACL_GET_PERMSET,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_get_qualifier),	SMB_VFS_OP_SYS_ACL_GET_QUALIFIER,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_get_file),			SMB_VFS_OP_SYS_ACL_GET_FILE,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_get_fd),			SMB_VFS_OP_SYS_ACL_GET_FD,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_clear_perms),		SMB_VFS_OP_SYS_ACL_CLEAR_PERMS,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_add_perm),			SMB_VFS_OP_SYS_ACL_ADD_PERM,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_to_text),			SMB_VFS_OP_SYS_ACL_TO_TEXT,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_init),				SMB_VFS_OP_SYS_ACL_INIT,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_create_entry),		SMB_VFS_OP_SYS_ACL_CREATE_ENTRY,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_set_tag_type),		SMB_VFS_OP_SYS_ACL_SET_TAG_TYPE,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_set_qualifier),	SMB_VFS_OP_SYS_ACL_SET_QUALIFIER,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_set_permset),		SMB_VFS_OP_SYS_ACL_SET_PERMSET,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_valid),			SMB_VFS_OP_SYS_ACL_VALID,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_set_file),			SMB_VFS_OP_SYS_ACL_SET_FILE,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_set_fd),			SMB_VFS_OP_SYS_ACL_SET_FD,			SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_delete_def_file),	SMB_VFS_OP_SYS_ACL_DELETE_DEF_FILE,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_get_perm),			SMB_VFS_OP_SYS_ACL_GET_PERM,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_free_text),		SMB_VFS_OP_SYS_ACL_FREE_TEXT,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_free_acl),			SMB_VFS_OP_SYS_ACL_FREE_ACL,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_sys_acl_free_qualifier),	SMB_VFS_OP_SYS_ACL_FREE_QUALIFIER,	SMB_VFS_LAYER_TRANSPARENT},

	// EA operations.
	{SMB_VFS_OP(skel_getxattr),		SMB_VFS_OP_GETXATTR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_lgetxattr),	SMB_VFS_OP_LGETXATTR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_fgetxattr),	SMB_VFS_OP_FGETXATTR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_listxattr),	SMB_VFS_OP_LISTXATTR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_llistxattr),	SMB_VFS_OP_LLISTXATTR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_flistxattr),	SMB_VFS_OP_FLISTXATTR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_removexattr),	SMB_VFS_OP_REMOVEXATTR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_lremovexattr),	SMB_VFS_OP_LREMOVEXATTR,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_fremovexattr),	SMB_VFS_OP_FREMOVEXATTR,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_setxattr),		SMB_VFS_OP_SETXATTR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_lsetxattr),	SMB_VFS_OP_LSETXATTR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(skel_fsetxattr),	SMB_VFS_OP_FSETXATTR,		SMB_VFS_LAYER_TRANSPARENT},
*/

/* VFS module registration */

NTSTATUS init_module(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "vfsx", vfsx_op_tuples);
}
