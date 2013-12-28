SuidPermissionProgram
=====================

This is an example of a SUID program to increase the permissions of other users, much like sudo.
Sume is the executable. All that is needed is for .sumeCfg file with the username of the allowed users to increase permissions in specific directories.

I clean the environment to make sure that it hasn't been tampered with.
I check to make sure that the configuration files permissions are properly set and that the user executing sume is configured to use it.

Check for the executable working directory using readlink "/proc/self/exe", argv[0] can be spoofed using different exec calls and 
spoofing the argument list. I needed a way of finding the directory of the executable which is in the home directory of the host user.

I also check each argument individually to make sure they are either flags or paths and not any other commands that could be executed.
I set the effective uid to the real uid before opening any files and after a file is successfully opened I then immediately reverse the 
process prior to doing an fstat on the file descriptor to protect against TOCTTOU attacks.

I only leave the the uid set as long as it has to be.
zero after malloc.
owner write permissions config.
fork() Permissions childexecute wait free.

ToDo
====
Generate config file on make.
Verify correct permissions on created binaries.
Minimize increased permissions.
Update for Mac OS X.


Considerations:
===============
As always have fun use at your own risk and don't blame me if things don't work. This is just a side project for learning.
If you actually need help with anything feel free to contact me and I'll gladly try to help. I am sublicensing this under
an MIT license, so go out use it and have fun.