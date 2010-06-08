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
