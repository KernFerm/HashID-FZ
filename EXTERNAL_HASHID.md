# External HashID on Raspberry Pi/Linux

External mode is optional. Native HashID remains fully available without another computer. External mode runs the genuine upstream `hashid` executable on a Raspberry Pi, spare Linux laptop, desktop, mini PC, or Linux VM while the Flipper acts as the UART controller and measured status display.

## Linux setup

```sh
sudo python3 -m pip install hashid pyserial
sudo install -d -m 0750 /var/lib/hashid-fz/output
sudo install -d -m 0755 /opt/hashid-fz
sudo cp companion/hashid_fz_bridge.py /opt/hashid-fz/
```

Place one input per line in `/var/lib/hashid-fz/input.txt`. The bridge only accepts HashID at `/usr/bin/hashid` or `/usr/local/bin/hashid`; verify with `command -v hashid`.

Connect a USB-to-3.3 V UART adapter to Flipper GPIO USART using crossed TX/RX and common ground. Never connect a 5 V UART signal. Match 115200, 230400, or 460800 baud in the app and bridge.

```sh
python3 /opt/hashid-fz/hashid_fz_bridge.py --port /dev/ttyUSB0 --baud 115200
```

On Flipper, open **External HashID**. Press OK to run normal identification, or enable **Extended candidates** first to run upstream `hashid -e`. Press OK while running to cancel. The genuine output is saved transactionally at `/var/lib/hashid-fz/output/hashid-report.txt`.

The HID1 protocol accepts only `HELLO`, `STATUS`, `RUN IDENTIFY`, `RUN EXTENDED`, and `CANCEL`. UART text is never passed to a shell.
