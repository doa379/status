# status

This utility outputs a string of system stats. The output can be piped through to xsetroot and/or used in .tmux.conf.

`sudo /usr/local/bin/status | while read LINE; do xsetroot -name "$LINE"; done`

`#/etc/sudoers.d/local`\
`%users ALL= NOPASSWD: /usr/local/bin/status`

In order to get power (RAPL) reports you need to run the utility with elevated privileges (ie,  root).
The `status_writer` script will output reports both to xsetroot, as well as a swap file for tmux or 
another program to read. The script can be invoked at system boot time by issuing the command:

`setsid /usr/local/bin/status_writer >/dev/null 2>&1 < /dev/null`
