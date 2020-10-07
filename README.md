# status

This utility outputs a string of vital system stats information. The output can be piped through to xsetroot or used in .tmux.conf.

`(status | while read LINE; do xsetroot -name "`echo $LINE`"; done)`

The utility also captures keyboard events and execs a corresponding shell script. The user should be part of the group "input". Programs requiring super user access (sudo) such as power states or other hardware access should make the necessary allowances in the sudoers file.

`
#/etc/sudoers.d/local <br />
%users ALL= NOPASSWD: /usr/local/bin/zzz, /usr/local/bin/backlight <br />
`
