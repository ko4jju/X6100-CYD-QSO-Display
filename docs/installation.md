# Installation guide

These directions describe the **tested X6100 R1CBU Linux + dual-USB ST7789 CYD** setup. They assume you can use a terminal on a Linux computer and can SSH into the radio.

> **Do not install or overwrite files on an X6100 that already has a working setup unless you intend to update it.** Back up existing scripts first.

## A. Download the project onto a Linux computer

```bash
git clone https://github.com/ko4jju/X6100-CYD-QSO-Display.git
cd X6100-CYD-QSO-Display
```


## B. Prepare and flash the CYD

1. Use the **dual-USB** ESP32 CYD with **ST7789** screen as described in the README.
2. Connect its **programming USB port** to the Linux computer using a data-capable USB cable.
3. No callsign editing is needed. The display starts with `NOCALL` and automatically receives the operator callsign from the X6100 after connection.
4. Install PlatformIO Core on the computer if it is not already installed.
5. Compile from the project's `cyd` directory:

   ```bash
   cd cyd
   pio run
   ```

6. Identify your ESP32 serial port, for example `/dev/ttyUSB0`, by checking `ls /dev/ttyUSB*` before and after plugging it in.
7. Flash the board, **substituting the correct port**:

   ```bash
   pio run --target upload --upload-port /dev/ttyUSB0
   ```

**Display configuration:** The build uses `cyd/config/User_Setup.h` with PlatformIO's `-include` option. It targets the tested dual-USB CYD; no manual editing of the downloaded TFT_eSPI library is needed.

**Touch warning:** A compiler message about `TOUCH_CS` being undefined in TFT_eSPI is expected; touch is handled by a separate XPT2046 library.

## Easier installation using the beta installer

**Status: Beta.** Compatibility checking has been tested on an X6100 running R1CBU firmware. Full installation and automatic recovery from failures still require testing.

On the Linux computer, return to the repository root (`cd ..` if you just built the CYD from the `cyd` directory). Then copy the complete installer folder to the X6100:

```bash
scp -r installer root@X6100:/tmp/
```

Connect to the radio:

```bash
ssh root@X6100
```

On the tested firmware, the default SSH password is `123`. Change this password using `passwd` if it is still set to the default.

**The remaining commands run on the X6100.**

Check compatibility without installing anything:

```bash
sh /tmp/installer/install_x6100.sh --check
```

If the checks pass, install the QSO monitor:

```bash
sh /tmp/installer/install_x6100.sh --install
```

The installer backs up existing scripts, installs the monitor and starts its background service.

Check the result:

```bash
/etc/init.d/S97cyd_qso status
tail -20 /tmp/cyd_qso.log
```

The CYD should display contacts and the callsign configured in R1CBU once connected to the X6100 USB HOST port and synchronized. Until synchronization, its header shows `NOCALL`.


### Manual installation

The following instructions remain available for operators who prefer to install each file themselves.

## C. Transfer scripts to the X6100

On the Linux computer, return to the repository root:

```bash
cd ..
```

Replace `X6100` below with the radio's IP address if its hostname does not resolve. Copy the four files:

```bash
scp x6100/x6100_cyd_sender.py root@X6100:/root/
scp x6100/x6100_cyd_watch.py root@X6100:/root/
scp x6100/x6100_cyd_run.sh root@X6100:/root/
scp x6100/S97cyd_qso root@X6100:/etc/init.d/
```

Then connect to the radio:

```bash
ssh root@X6100
```

**All remaining commands in sections D–F run on the X6100, not on the Linux computer.**

## D. Make launcher scripts executable

```bash
chmod +x /root/x6100_cyd_run.sh /etc/init.d/S97cyd_qso
```

The radio needs `/usr/bin/python3`, the BusyBox `start-stop-daemon` command, and the log file `/mnt/ft_log.adi`. These were present on the tested radio; check your own R1CBU version.

## E. Plug in the CYD and test

Connect the CYD's USB serial connection to the **X6100 USB HOST** port, using a known-good data cable.

Check that the CH340 adapter is recognized:

```bash
lsusb
ls -l /dev/ttyUSB*
```

Test ADIF parsing *without sending anything*:

```bash
python3 /root/x6100_cyd_sender.py --dry-run
```

Start the background service:

```bash
/etc/init.d/S97cyd_qso start
```

After about 20 seconds, check:

```bash
/etc/init.d/S97cyd_qso status
tail -20 /tmp/cyd_qso.log
```

A successful update includes lines similar to:

```text
Synchronizing: 31 QSOs
CYD updated successfully
```

Your actual QSO count will vary.

## F. Confirm automatic startup

Save any radio settings or other work, then reboot the X6100 normally with the CYD connected. Check:

```bash
/etc/init.d/S97cyd_qso status
tail -20 /tmp/cyd_qso.log
```

The tested BusyBox init process starts executable scripts named `S??*` in `/etc/init.d/`, including `S97cyd_qso`. Other firmware builds may behave differently.

## Troubleshooting

**CYD is blank / no contacts:** Check `/mnt/ft_log.adi`, the USB cable, and `/tmp/cyd_qso.log`.

```bash
ls -l /mnt/ft_log.adi
lsusb
ls -l /dev/ttyUSB*
tail -30 /tmp/cyd_qso.log
```

**USB serial port changes from `/dev/ttyUSB0` to `/dev/ttyUSB1`:** This is normal. The sender and watcher search for the CH340 vendor/product IDs (`1a86:7523`) automatically.

**After a reboot the monitor runs but screen is empty:** Check whether the radio has enumerated the CYD and whether the monitor reported `CYD updated successfully`. A faulty data cable or a delayed USB connection can cause transient failures.

**Screen scrambled or blank after flashing:** Confirm you purchased the same dual-USB ST7789 hardware. Other CYD versions often use different wiring and display controllers.

**Stop service:** `/etc/init.d/S97cyd_qso stop`

**Restart service:** `/etc/init.d/S97cyd_qso restart`

## Notes for developers

The current serial protocol uses newline-separated `CALLSIGN|<operator>`, `BEGIN`, `Q|...`, and `END` records at 115200 baud. The X6100 reads the configured callsign from `/mnt/params.db` (`params` table, `callsign` setting); the CYD displays `NOCALL` until a valid callsign arrives. The CYD shows the latest 300 contacts, five per page. The radio's original ADIF file is never modified by the sender or watcher.

## SSH Login Information

Tested R1CBU firmware SSH login:

- Username: root
- Default password: 123
- Hostname: X6100

Connect using: ssh root@X6100

If the hostname does not work, substitute the radio's IP address.

The password will not appear while typing.

SECURITY: Change the default password using the passwd command.
The default password may differ on other firmware versions.
