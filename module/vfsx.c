/*
 * VFSX - External VFS Bridge
 * This transparent VFS module sends VFS operations over a Unix domain
 * socket for external handling.
 *
 * Copyright (C) Steven Farley 2004
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

/* Function prototypes */

static int vfsx_connect(vfs_handle_struct *handle, connection_struct *conn, const char *svc, const char *user);
static void vfsx_disconnect(vfs_handle_struct *handle, connection_struct *conn);
static DIR *vfsx_opendir(vfs_handle_struct *handle, connection_struct *conn, const char *fname);
static int vfsx_mkdir(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode);
static int vfsx_rmdir(vfs_handle_struct *handle, connection_struct *conn, const char *path);
static int vfsx_open(vfs_handle_struct *handle, connection_struct *conn, const char *fname, int flags, mode_t mode);
static int vfsx_close(vfs_handle_struct *handle, files_struct *fsp, int fd);
static int vfsx_rename(vfs_handle_struct *handle, connection_struct *conn, const char *old, const char *new);
static int vfsx_unlink(vfs_handle_struct *handle, connection_struct *conn, const char *path);
static int vfsx_chmod(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode);
static int vfsx_chmod_acl(vfs_handle_struct *handle, connection_struct *conn, const char *name, mode_t mode);
static int vfsx_fchmod(vfs_handle_struct *handle, files_struct *fsp, int fd, mode_t mode);
static int vfsx_fchmod_acl(vfs_handle_struct *handle, files_struct *fsp, int fd, mode_t mode);

static int vfsx_execute(const char *buf, int count);
static void vfsx_write_file(const char *buf);
static int vfsx_write_socket(const char *buf, int close);

/* VFS operations */

static vfs_op_tuple vfsx_op_tuples[] = {

	/* Disk operations */

	{SMB_VFS_OP(vfsx_connect), 	SMB_VFS_OP_CONNECT, 	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_disconnect), 	SMB_VFS_OP_DISCONNECT, 	SMB_VFS_LAYER_TRANSPARENT},

	/* Directory operations */

	{SMB_VFS_OP(vfsx_opendir), 	SMB_VFS_OP_OPENDIR, 	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_mkdir), 		SMB_VFS_OP_MKDIR, 	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_rmdir), 		SMB_VFS_OP_RMDIR, 	SMB_VFS_LAYER_TRANSPARENT},

	/* File operations */

	{SMB_VFS_OP(vfsx_open), 		SMB_VFS_OP_OPEN, 	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_close), 		SMB_VFS_OP_CLOSE, 	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_rename), 		SMB_VFS_OP_RENAME, 	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_unlink), 		SMB_VFS_OP_UNLINK, 	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_chmod), 		SMB_VFS_OP_CHMOD, 	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_fchmod), 		SMB_VFS_OP_FCHMOD, 	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_chmod_acl), 	SMB_VFS_OP_CHMOD_ACL, 	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(vfsx_fchmod_acl), 	SMB_VFS_OP_FCHMOD_ACL, 	SMB_VFS_LAYER_TRANSPARENT},

	/* Finish VFS operations definition */

	{SMB_VFS_OP(NULL), 			SMB_VFS_OP_NOOP, 	SMB_VFS_LAYER_NOOP}
};

/* VFS module registration */

NTSTATUS init_module(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "vfsx", vfsx_op_tuples);
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

 /* Internal functions */

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
