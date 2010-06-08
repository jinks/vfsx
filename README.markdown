# VFSX – Samba VFS External Bridge

## Version 0.2

### Overview

VFSX is a transparent Samba Virtual File System (VFS) module which forwards operations to a process on the same machine for handing outside of the Samba daemon process (smbd). The external handler can be implemented in any language with support for Unix domain sockets (Python, Ruby, Perl, Java with Jtux) which is how VFSX and its external process communicates.

The advantage of using VFSX over a pure VFS module is that any programming language can be used to implement the transparent operations. VFS modules linked directly into the Samba daemon must be written in C or C++. while VFSX lets the developer implement a transparent module in his favorite higher-level language, with all the advantages of that language, outside of the smbd process.

The following figure illustrates a typical file server configuration with VFSX:

![Config][1]

For Samba shares configured with VFSX as a VFS module, all client requests to manipulate files and directories will first be sent to the external handler process. For every VFS operation invoked by the Samba daemon, VFSX sends to the handler the name of the operation, the local directory path of the shared SMB service, the ID of the calling user, and any additional arguments required for the operation. The handler may respond to any operation for which it is designed. Note that since VFSX is a transparent module, file contents may not be passed between the VFSX module and its handler; The Samba daemon must still handle the file system I/O directly. However, the external handler may choose to reject an operation and return an error code which will be passed on to the client.

Included with VFSX is an external handler written in Python. Developers can extend this implementation to provide custom operation handling.

### Deploying the VFSX Module
VFSX requires Samba 3.0 or higher. All directory and file paths described in the following instructions are based on Fedora Core 2 so be sure to substitute paths as appropriate for your Linux distribution.

1. Install Samba 3.0, including the source distribution.
2. Download the VFSX source distribution: `git clone git://gitorious.org/vfsx/mainline.git.`
3. Edit  `vfsx/module/Makefile` and modify the `SAMBA\_SOURCE` variable to point to your Samba source directory.
4. Build the VFSX shared library: `make`
5. Attach VFSX to a Samba shared directory by adding the VFSX module name to the share's configuration parameters (found in `/etc/samba/smb.conf`). 
For example:  
`[myshare]`  
`comment = VFSX-Aware Shared Directory`  
`path = /home/myuser/shared/`  
`valid users = myuser`  
`read only = No`  
`vfs objects = vfsx`
6. Deploy the VFSX shared library (as root):  
`cp vfsx/module/vfsx.so /usr/lib/samba/vfs/`
7. Restart Samba (as root):  
`/etc/rc.d/init.d/smb restart`
8. Run the Python external event handler:  
`python vfsx/python/vfsx.py`
9. <i>Access the share using smbclient or from a Windows system. By default the Python handler prints debug activity messages to the console. If the module has problems communicating with the external handler, error messages are written to syslog.</i>

### Developing a Custom VFSX Handler with Python

1. Extend
2. Run

### Links

* [VFSX ][2]
* [Original VFSX Project Page at SourceForge][3]
* [Samba Home Page][4]
* [ Samba VFS Module Configuration][5]
* [ Samba VFS Module Developers Guide][6]
* [Python Home Page][7]

### License
VFSX is distributed under the open source [Mozilla Public License][8]. The file  `vfsx/LICENSE` contains the license terms.

Copyright (C) 2004 Steven R. Farley. All rights reserved.  
Copyright (C) 2009 Alexander Duscheleit.

  [1]: http://gitorious.org/vfsx/vfsx/blobs/raw/master/docs/config.png
  [2]: http://gitorious.org/vfsx
  [3]: http://sourceforge.net/projects/vfsx/
  [4]: http://www.samba.org/
  [5]: http://www.samba.org/samba/docs/man/Samba-HOWTO-Collection/VFS.html
  [6]: http://www.samba.org/samba/docs/man/Samba-Developers-Guide/vfs.html
  [7]: http://www.python.org/
  [8]: http://www.mozilla.org/MPL/MPL-1.1.html
