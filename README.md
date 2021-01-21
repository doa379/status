# status

This utility outputs a string of system stats. The output can be piped through to xsetroot or used in .tmux.conf.
`(sudo /usr/local/bin/status | while read LINE; do xsetroot -name "$(echo $LINE)"; done)`

`#/etc/sudoers.d/local`\
`%users ALL= NOPASSWD: /usr/local/bin/status`
