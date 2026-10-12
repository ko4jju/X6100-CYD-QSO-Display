#!/bin/sh
exec /usr/bin/python3 -u /root/x6100_cyd_watch.py \
    >> /tmp/cyd_qso.log 2>&1
