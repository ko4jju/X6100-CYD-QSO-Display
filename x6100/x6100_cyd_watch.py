#!/usr/bin/env python3
import os
import time
import x6100_cyd_sender as sender

LOG = "/mnt/ft_log.adi"
PORT = "/dev/ttyUSB0"
INTERVAL = 15

last_callsign = None
last_log = None
last_usb = None

print("X6100 CYD automatic QSO monitor", flush=True)
print("Checking every 15 seconds", flush=True)

while True:
    try:
        log_info = os.stat(LOG)
        PORT = sender.detect_port()
        usb_info = os.stat(PORT)

        log_state = (
            log_info.st_size,
            log_info.st_mtime_ns,
            log_info.st_ino
        )

        usb_state = (
            usb_info.st_ino,
            usb_info.st_ctime_ns,
            usb_info.st_rdev
        )

        current_callsign = sender.read_callsign()
        if log_state != last_log or usb_state != last_usb or current_callsign != last_callsign:
            records = sender.read_adif()

            print(
                "Synchronizing:",
                len(records),
                "QSOs",
                flush=True
            )

            sender.send(records)

            last_callsign = current_callsign
            last_log = log_state
            last_usb = usb_state

            print("CYD updated successfully", flush=True)

    except (OSError, ValueError) as e:
        print("Waiting or retrying:", e, flush=True)
        last_usb = None

    except Exception as e:
        print("Synchronization error:", e, flush=True)
        last_usb = None

    time.sleep(INTERVAL)
