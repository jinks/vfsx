CC		= gcc
CFLAGS		= -g -O2 
CPPFLAGS	=  
LDFLAGS		= 
LDSHFLAGS	= -shared
INSTALLCMD	= /usr/bin/install -c
SAMBA_SOURCE	= ../../../samba-3.0.3/source
SHLIBEXT	= so
OBJEXT		= o 
FLAGS		=  $(CFLAGS) -I/usr/include -Iinclude -I$(SAMBA_SOURCE)/include -I$(SAMBA_SOURCE)/ubiqx -I$(SAMBA_SOURCE)/smbwrapper  -I. $(CPPFLAGS) -I$(SAMBA_SOURCE)


prefix		= /usr/lib/samba
libdir		= ${prefix}/lib

VFS_LIBDIR	= $(libdir)/vfs

# Auto target
default: $(patsubst %.c,%.$(SHLIBEXT),$(wildcard *.c))

# Pattern rules

%.$(SHLIBEXT): %.$(OBJEXT)
	@echo "Linking $@"
	@$(CC) $(LDSHFLAGS) $(LDFLAGS) -o $@ $<

%.$(OBJEXT): %.c
	@echo "Compiling $<"
	@$(CC) $(FLAGS) -c $<


install: default
	$(INSTALLCMD) -d $(VFS_LIBDIR)
	$(INSTALLCMD) -m 755 *.$(SHLIBEXT) $(VFS_LIBDIR)

# Misc targets
clean:
	rm -rf .libs
	rm -f core *~ *% *.bak *.o *.$(SHLIBEXT)


