#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2026 Wireless-Tag
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import hashlib
import os
import subprocess
import sys
from pathlib import Path


FIRMWARE_REL_PATH = Path("p4_flash") / "firmware" / "P4FlashC61_usb2uart_esp32p4_merged.bin"
SHA256SUMS_REL_PATH = Path("p4_flash") / "firmware" / "SHA256SUMS"
FLASH_OFFSET = "0x0"
COLOR_RED = "\033[31m"
COLOR_GREEN = "\033[32m"
COLOR_YELLOW = "\033[33m"
COLOR_RESET = "\033[0m"


class P4FlashError(Exception):
    pass


def _tools_dir():
    return Path(__file__).resolve().parent


def _firmware_path():
    return _tools_dir() / FIRMWARE_REL_PATH


def _sha256sums_path():
    return _tools_dir() / SHA256SUMS_REL_PATH


def _read_expected_sha256():
    sha256sums = _sha256sums_path()
    firmware_name = _firmware_path().name

    if not sha256sums.is_file():
        raise P4FlashError("SHA256SUMS not found: {}".format(sha256sums))

    for line in sha256sums.read_text(encoding="utf-8").splitlines():
        fields = line.strip().split()
        if len(fields) >= 2 and Path(fields[1]).name == firmware_name:
            return fields[0].lower()

    raise P4FlashError("Expected SHA256 for {} not found".format(firmware_name))


def _calculate_sha256(path):
    digest = hashlib.sha256()

    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)

    return digest.hexdigest()


def verify_firmware():
    firmware = _firmware_path()

    if not firmware.is_file():
        raise P4FlashError("Firmware image not found: {}".format(firmware))

    expected = _read_expected_sha256()
    actual = _calculate_sha256(firmware)
    if actual != expected:
        raise P4FlashError(
            "Firmware SHA256 mismatch:\n"
            "  expected: {}\n"
            "  actual:   {}".format(expected, actual)
        )

    print("Firmware SHA256 verified: {}".format(actual))


def list_ports():
    try:
        from serial.tools import list_ports as serial_list_ports
    except ImportError as e:
        raise P4FlashError("pyserial is not available in this Python environment") from e

    return sorted(serial_list_ports.comports(), key=lambda item: item.device)


def print_ports(ports):
    if not ports:
        print("No serial ports found.")
        return

    print("{}Available serial ports:{}".format(COLOR_GREEN, COLOR_RESET))
    for index, port in enumerate(ports):
        description = port.description or "n/a"
        hwid = port.hwid or "n/a"
        print("{}  [{}] {} | {} | {}{}".format(
            COLOR_GREEN, index, port.device, description, hwid, COLOR_RESET
        ))
    print("{}\n\rPlease enter the serial port number to use, then press Enter:{}".format(COLOR_GREEN, COLOR_RESET))


def choose_port():
    env_port = os.environ.get("ESPPORT")
    if env_port:
        print("Using ESPPORT from environment: {}".format(env_port))
        return env_port

    print("{}Connect the WT9932P4C61-TINY FUSB port for ESP32-P4 flashing.{}".format(
        COLOR_YELLOW, COLOR_RESET
    ))
    print("{}Do not use the HUSB TinyUSB CDC port in this step.{}".format(COLOR_YELLOW, COLOR_RESET))
    print("")

    ports = list_ports()
    print_ports(ports)
    if not ports:
        raise P4FlashError("Connect the WT9932P4C61-TINY P4 USB-JTAG/Serial port and try again")

    while True:
        selection = input("Select the ESP32-P4 port number: ").strip()
        if not selection:
            raise P4FlashError("No port selected")

        try:
            index = int(selection, 10)
        except ValueError:
            print("Please enter a port number from the list.")
            continue

        if 0 <= index < len(ports):
            return ports[index].device

        print("Port number out of range.")


def confirm_overwrite(port):
    print("")
    print("{}WARNING: This command will overwrite the current ESP32-P4 firmware.{}".format(
        COLOR_RED, COLOR_RESET
    ))
    print("{}Target board: WT9932P4C61-TINY{}".format(COLOR_RED, COLOR_RESET))
    print("{}P4 port: {}{}".format(COLOR_RED, port, COLOR_RESET))
    print("{}New firmware: ESP32-P4 USB-UART bridge for flashing/debugging onboard ESP32-C61{}".format(
        COLOR_RED, COLOR_RESET
    ))
    print("")
    print("{}Type 'y' to continue, or press Enter to cancel. [y/N]:{}".format(
        COLOR_RED, COLOR_RESET
    ))
    answer = input("Continue? [Y/N(N)]: ").strip().lower()
    if answer != "y":
        raise P4FlashError("Aborted by user")


def run_esptool(port, args):
    command = [sys.executable, "-m", "esptool", "--chip", "esp32p4", "-p", port]
    command.extend(args)
    print("Running: {}".format(" ".join(command)))
    subprocess.run(command, check=True)


def main():
    parser = argparse.ArgumentParser(
        description="Flash the WT9932P4C61-TINY ESP32-P4 USB-UART bridge firmware."
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="List serial ports and exit.",
    )
    args = parser.parse_args()

    try:
        if args.list:
            print_ports(list_ports())
            return 0

        verify_firmware()
        port = choose_port()
        confirm_overwrite(port)
        run_esptool(port, ["chip-id"])
        run_esptool(port, ["--before=default-reset", "--after=hard-reset",
                           "write-flash", "--flash-mode", "dio", "--flash-freq", "80m",
                           "--flash-size", "2MB", FLASH_OFFSET, str(_firmware_path())])
    except subprocess.CalledProcessError as e:
        print("ERROR: esptool failed with exit code {}".format(e.returncode), file=sys.stderr)
        return e.returncode
    except (KeyboardInterrupt, EOFError):
        print("\nAborted by user", file=sys.stderr)
        return 1
    except P4FlashError as e:
        print("ERROR: {}".format(e), file=sys.stderr)
        return 1

    print("")
    print("ESP32-P4 bridge firmware flashed successfully.")
    print("{}Unplug FUSB, then connect HUSB. Use HUSB to power the board and flash/debug ESP32-C61.{}".format(
        COLOR_YELLOW, COLOR_RESET
    ))
    return 0


if __name__ == "__main__":
    sys.exit(main())
