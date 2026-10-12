#!/usr/bin/env python3
import os
from pathlib import Path
import re
import sys
import time
import termios
import tty

LOG = "/mnt/ft_log.adi"
PORT = "/dev/ttyUSB0"

def detect_port():
    import glob

    for path in sorted(glob.glob("/sys/bus/usb-serial/devices/ttyUSB*")):
        name = os.path.basename(path)
        try:
            real = os.path.realpath(path)
            current = Path(real)
            for parent in [current] + list(current.parents):
                vid = parent / "idVendor"
                pid = parent / "idProduct"
                if vid.exists() and pid.exists():
                    if (vid.read_text().strip().lower() == "1a86"
                        and pid.read_text().strip().lower() == "7523"):
                        return "/dev/" + name
        except OSError:
            continue

    raise FileNotFoundError("CYD CH340 USB serial device not found")


MAX_QSOS = 300

TAG = re.compile(
    rb"<\s*([A-Za-z0-9_]+)\s*(?::\s*(\d+)"
    rb"\s*(?::[^>]*)?)?\s*>",
    re.I
)

def read_adif():
    with open(LOG, "rb") as f:
        data = f.read()

    records = []
    current = {}
    pos = 0

    while True:
        match = TAG.search(data, pos)
        if not match:
            break

        name = match.group(1).decode("ascii").upper()
        length = match.group(2)
        pos = match.end()

        if name == "EOH":
            current = {}
            continue

        if name == "EOR":
            if current.get("CALL"):
                records.append(current)
            current = {}
            continue

        if length is not None:
            n = int(length)
            if n > len(data) - pos:
                break
            value = data[pos:pos+n]
            current[name] = value.decode(
                "utf-8", errors="replace"
            ).strip()
            pos += n

    return records

def clean(value):
    return str(value or "").replace(
        "|", " "
    ).replace("\r", " ").replace("\n", " ")[:35]

def format_qso(q):
    call = clean(q.get("CALL"))
    band = clean(q.get("BAND"))
    mode = clean(q.get("SUBMODE") or q.get("MODE"))
    raw_time = q.get("TIME_ON") or q.get("TIME_OFF") or ""
    digits = re.sub(r"\D", "", raw_time)
    utc = digits[:2] + ":" + digits[2:4] if len(digits) >= 4 else ""
    freq = clean(q.get("FREQ"))
    grid = clean(q.get("GRIDSQUARE"))
    rst = clean(q.get("RST_SENT"))
    rcvd = clean(q.get("RST_RCVD"))

    raw_date = re.sub(r"\D", "", q.get("QSO_DATE") or "")
    date = (raw_date[:4] + "-" + raw_date[4:6] + "-"
            + raw_date[6:8]) if len(raw_date) >= 8 else ""

    return "Q|" + "|".join(
        [call, band, mode, utc, freq, grid, rst, date, rcvd]
    ) + "\n"

def send(records):
    fd = os.open(detect_port(), os.O_RDWR | os.O_NOCTTY)
    old = termios.tcgetattr(fd)

    try:
        tty.setraw(fd)
        settings = termios.tcgetattr(fd)
        settings[4] = termios.B115200
        settings[5] = termios.B115200
        termios.tcsetattr(fd, termios.TCSANOW, settings)

        # Allow the CYD serial interface to settle.
        time.sleep(2)

        os.write(fd, b"BEGIN\n")
        time.sleep(0.1)

        # Display newest contacts first.
        for q in reversed(records[-MAX_QSOS:]):
            os.write(fd, format_qso(q).encode("utf-8"))
            time.sleep(0.04)

        os.write(fd, b"END\n")
        termios.tcdrain(fd)

    finally:
        termios.tcsetattr(fd, termios.TCSANOW, old)
        os.close(fd)

def main():
    records = read_adif()
    print("ADIF file:", LOG)
    print("Total QSOs:", len(records))
    print("Sending QSOs:", min(len(records), MAX_QSOS))

    if "--dry-run" in sys.argv:
        print("First five contacts:")
        for q in records[:5]:
            print(format_qso(q).strip())
        print("No data transmitted.")
        return

    send(records)
    print("Transfer complete.")

if __name__ == "__main__":
    main()
