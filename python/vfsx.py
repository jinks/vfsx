# This module contains classes for implementing a custom VFSX handler.
# Copyright (C) Steven R. Farley 2004

import SocketServer
import os
import os.path
import logging
import traceback

# General error.  VFS operation should not proceed.
FAIL_ERROR = -1

# Authorization error.  User is not allowed to execute operation.
# VFS operation should not proceed.
FAIL_AUTHORIZATION = -2

# Operation not implemented.
FAIL_NOT_IMPLEMENTED = -3

# Successful operation.  VFS operation can proceed.
SUCCESS_TRANSPARENT = 0

# The Unix domain socket file
SOCKET_FILE = "/tmp/vfsx-socket"

# Logger for this module
logging.basicConfig()
log = logging.getLogger("vfsx")
log.setLevel(logging.DEBUG)

class VFSOperationResult(object):

	def __init__(self, status):
		self.status = status


class VFSModuleSession(object):

	# The VFSModuleSession subclass of new instances created
	# in getSession()
	__sessionClass = None

	# All connected sessions
	__sessions = {}

	def setSessionClass(sessionClass):
		VFSModuleSession.__sessionClass = sessionClass
	setSessionClass = staticmethod(setSessionClass)

	def getSessionClass():
		return VFSModuleSession.__sessionClass
	getSessionClass = staticmethod(getSessionClass)

	def getSession(origpath, user):
		sessions = VFSModuleSession.__sessions
		key = (origpath, user)
		if sessions.has_key(key):
			session = sessions[key]
			log.debug("Existing session: %s" % session)
		else:
			session = VFSModuleSession.__sessionClass(*key)
			sessions[key] = session
			log.debug("New session: %s" % session)
		return session
	getSession = staticmethod(getSession)

	def removeSession(session):
		key = (session.origpath, session.user)
		del VFSModuleSession.__sessions[key]
		log.debug("Removed session: %s" % session)
	removeSession = staticmethod(removeSession)

	# Instance methods

	def __init__(self, origpath, user):
		self.origpath = origpath
		self.user = user

	def __str__(self):
		return "user = %s, origpath = %s" % (self.user, self.origpath)

	# This is the default method to call when a specific operation method is
	# not found on the subclass.  The subclass can override this method to
	# return a different result.
	def defaultOperation(self, *args):
		return VFSOperationResult(NOT_IMPLEMENTED)

	# VFS connect operations

	def connect(self):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def disconnect(self):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	# VFS directory operations

	def opendir(self, path):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def readdir(self, path):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def mkdir(self, path, mode):
		VFSOperationResult(SUCCESS_TRANSPARENT)

	def rmdir(self, path):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def closedir(self, path):
		return VFSOperationResult(SUCCESS_TRANSPARENT)


	# VFS file operations

	def open(self, path, flags, mode):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def close(self, path):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def read(self, path):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def pread(self, path):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def write(self, path):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def pwrite(self, path):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def lseek(self, path):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def rename(self, oldPath, newPath):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def fsync(self, path):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def stat(self, path):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def fstat(self, path):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def lstat(self, path):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def unlink(self, path):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def chmod(self, path, mode):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def fchmod(self, path, mode):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def chown(self, path, uid, guid):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def fchown(self, path, uid, guid):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def chdir(self, path):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def getwd(self):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def utime(self, path):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def ftruncate(self, path, offset):
		return VFSOperationResult(SUCCESS_TRANSPARENT)

	def realpath(self, path, resolvedPath):
		return VFSOperationResult(SUCCESS_TRANSPARENT)


# Expects a string of the form "operation:user:origpath:arg1,arg2,arg3" where
# "operation" is the VFS operation.  It should exactly match the method on
# VFSModuleSession.  A new VFSModuleSession is created for each "connect"
# operation.
class VFSHandler(SocketServer.BaseRequestHandler):

	def handle(self):
		log.debug("-- Open Connection --")
		while True:
			msg = self.request.recv(512)
			if not msg: break
			log.debug(msg)
			# Handle message-parsing and operation execution error here.
			# Socket communication errors should be propagated.
			try:
				(operation, user, origpath, args) = self.__parseMessage(msg)
				result = self.__callOperation(operation, user, origpath, args)
			except Exception, e:
				result = VFSOperationResult(FAIL_ERROR)
				log.exception(e)
			self.request.send("%d" % result.status)

		# The client probably closed the connection.
		self.request.close()
		log.debug("Close Connection")

	def __parseMessage(self, msg):
		parts = msg.split(":")
		(operation, user, origpath) = parts[0:3]
		log.debug("  operation = '%s' user = '%s' origpath = '%s'" %
			(operation, user, origpath))
		args = []
		if len(parts) > 3:
			args = parts[3].split(",")
			log.debug("  args = '%s'" % parts[3])
		return (operation, user, origpath, args)

	def __callOperation(self, operation, user, origpath, args):
		session = VFSModuleSession.getSession(origpath, user)
		if operation == "disconnect":
			VFSModuleSession.removeSession(session)
		sessionClass = VFSModuleSession.getSessionClass()
		try:
			method = eval("sessionClass.%s" % operation)
		except AttributeError:
			method = sessionClass.defaultOperation
		return method(session, *args)


def runServer(vfsSessionClass):
	if os.path.exists(SOCKET_FILE):
		os.unlink(SOCKET_FILE)
	VFSModuleSession.setSessionClass(vfsSessionClass)
	server = SocketServer.UnixStreamServer(SOCKET_FILE, VFSHandler)
	server.serve_forever()

# Initialize default VFSModuleSession class
VFSModuleSession.setSessionClass(VFSModuleSession)

if __name__ == "__main__":
	runServer(VFSModuleSession)

"""
	/* Directory operations */

	SMB_VFS_OP_OPENDIR,
	SMB_VFS_OP_READDIR,
	SMB_VFS_OP_MKDIR,
	SMB_VFS_OP_RMDIR,
	SMB_VFS_OP_CLOSEDIR,

	/* File operations */

	SMB_VFS_OP_OPEN,
	SMB_VFS_OP_CLOSE,
	SMB_VFS_OP_READ,
	SMB_VFS_OP_PREAD,
	SMB_VFS_OP_WRITE,
	SMB_VFS_OP_PWRITE,
	SMB_VFS_OP_LSEEK,
	SMB_VFS_OP_SENDFILE,
	SMB_VFS_OP_RENAME,
	SMB_VFS_OP_FSYNC,
	SMB_VFS_OP_STAT,
	SMB_VFS_OP_FSTAT,
	SMB_VFS_OP_LSTAT,
	SMB_VFS_OP_UNLINK,
	SMB_VFS_OP_CHMOD,
	SMB_VFS_OP_FCHMOD,
	SMB_VFS_OP_CHOWN,
	SMB_VFS_OP_FCHOWN,
	SMB_VFS_OP_CHDIR,
	SMB_VFS_OP_GETWD,
	SMB_VFS_OP_UTIME,
	SMB_VFS_OP_FTRUNCATE,
	SMB_VFS_OP_LOCK,
	SMB_VFS_OP_SYMLINK,
	SMB_VFS_OP_READLINK,
	SMB_VFS_OP_LINK,
	SMB_VFS_OP_MKNOD,
	SMB_VFS_OP_REALPATH,
"""
