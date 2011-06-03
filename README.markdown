Encapsulate
===========

Linux utility to isolate a process and its children while providing a full environment easily.
Makes use of Linux's namespace capabilities and is thus totally unportable. It also might
require more Linux kernel options than you have enabled.

Usage
-----
`encapsulate new-root writable-subtree command args...`

Isolation
---------
encapsulate:
 * detaches itself (and its children) from the system's mount point table, IPC table, process ID table and network stack instance,
 * mounts the current filesystem view to `new-root`,
 * marks it read-only,
 * mounts `writable-subtree` at its "native" location into the new directory hierarchy,
 * chroots to `new-root`,
 * chdirs to the current directory (but inside the chroot),
 * setuids back to the current user,
 * and finally calls `command` with `args`

The result is that `command` runs in a system similar to the real one with a couple of exceptions. First, only files below
`writable-subtree` are writable, everything else (including /tmp, unless that's the directory you choose) is read-only.
Second, `command` can't inspect many aspects of the system (such as currently running processes) or interact with processes
easily. Third, network is blocked, so if `command` attempts to run a spam-bot, it will fail.
