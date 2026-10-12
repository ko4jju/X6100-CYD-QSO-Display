#!/bin/sh
# X6100 CYD QSO Display installer for R1CBU Buildroot
set -eu
MODE="${1:---check}"
case "$MODE" in --check|--install) ;; *) echo 'Usage: sh install_x6100.sh --check|--install' >&2; exit 2 ;; esac
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
FILES="$HERE/x6100"
fail() { echo "ERROR: $*" >&2; exit 1; }
info() { echo "  $*"; }
[ "$(id -u)" -eq 0 ] || fail 'Run as root on the X6100'
[ -x /usr/sbin/x6100_gui ] || fail 'R1CBU GUI not found'
[ -f /etc/init.d/rcS ] || fail 'Expected startup system not found'
[ -d /mnt ] || fail '/mnt directory missing'
for cmd in python3 cp chmod mkdir date start-stop-daemon mv rm sleep; do
  command -v "$cmd" >/dev/null 2>&1 || fail "Missing command: $cmd"
done
python3 -c 'import termios,tty,pathlib' || fail 'Python requirements missing'
[ -w /root ] && [ -w /etc/init.d ] || fail 'Install locations are not writable'
info 'R1CBU GUI and required system tools: found'
info 'Firmware version: not verified; other releases untested'
if [ -f /mnt/ft_log.adi ]; then info 'ADIF log: present'; else info 'ADIF log: absent (monitor will retry)'; fi
if [ "$MODE" = '--check' ]; then
  info 'Check complete. No files modified.'
  exit 0
fi
for f in x6100_cyd_sender.py x6100_cyd_watch.py x6100_cyd_run.sh S97cyd_qso; do
  [ -f "$FILES/$f" ] || fail "Missing package file: x6100/$f"
done
python3 -c 'import ast,sys; [ast.parse(open(p).read(),filename=p) for p in sys.argv[1:]]' \
  "$FILES/x6100_cyd_sender.py" "$FILES/x6100_cyd_watch.py" || fail 'Python syntax check failed'
BACKUP="/root/cyd-install-backup-$(date +%Y%m%d-%H%M%S)-$$"
mkdir -p "$BACKUP"
TARGETS='/root/x6100_cyd_sender.py /root/x6100_cyd_watch.py /root/x6100_cyd_run.sh /etc/init.d/S97cyd_qso'
for dest in $TARGETS; do
  filename=${dest##*/}
  if [ -e "$dest" ]; then
    cp -p "$dest" "$BACKUP/$filename" || fail "Backup failed for $dest"
  else
    : > "$BACKUP/.absent-$filename"
  fi
done
info "Backup: $BACKUP"
COMMITTED=0
rollback() {
  [ "$COMMITTED" -eq 0 ] || return 0
  echo 'Installation interrupted or failed. Restoring previous files.' >&2
  for dest in $TARGETS; do
    filename=${dest##*/}
    if [ -f "$BACKUP/.absent-$filename" ]; then
      rm -f "$dest"
    elif [ -f "$BACKUP/$filename" ]; then
      cp -p "$BACKUP/$filename" "$dest" || echo "Manual restore required: $dest" >&2
    fi
  done
  if [ -x /etc/init.d/S97cyd_qso ] && [ -f "$BACKUP/S97cyd_qso" ]; then
    /etc/init.d/S97cyd_qso start || true
  fi
}
trap 'rollback' 0
trap 'exit 1' 1 2 3 15
if [ -x /etc/init.d/S97cyd_qso ]; then
  /etc/init.d/S97cyd_qso stop || fail 'Could not stop existing service'
fi
for f in x6100_cyd_sender.py x6100_cyd_watch.py x6100_cyd_run.sh; do
  cp "$FILES/$f" "/root/$f" || fail "Could not install $f"
done
cp "$FILES/S97cyd_qso" /etc/init.d/S97cyd_qso || fail 'Could not install service'
chmod 755 /root/x6100_cyd_run.sh /etc/init.d/S97cyd_qso || fail 'chmod failed'
/etc/init.d/S97cyd_qso start || fail 'Could not start service'
sleep 2
PIDFILE=/run/cyd_qso.pid
[ -f "$PIDFILE" ] || fail 'Service has no PID file'
PID=$(cat "$PIDFILE")
kill -0 "$PID" 2>/dev/null || fail 'Service exited unexpectedly'
COMMITTED=1
trap - 0 1 2 3 15
info 'Installation complete; monitor is running.'
info 'Check: tail -20 /tmp/cyd_qso.log'
info "Backup: $BACKUP"
