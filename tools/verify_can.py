"""Hardware acceptance test for the F072 CAN firmware and attached CAN1 box.

Derives SRAM symbol addresses from the exact ELF. Does not drive LED commands.
Writes raw MCU snapshots and a JSON report alongside the flash backup.
"""
import argparse
import json
import re
import struct
import subprocess
import time
from pathlib import Path

from can_box import CanBox

ROOT = Path(__file__).resolve().parent.parent
SYMBOLS = ["g_led_cmd", "g_can_rx_count", "g_led_show_count", "g_can_cmd_count",
           "g_can_tx_count", "g_can_error_count", "g_can_last_error",
           "g_can_last_error_esr", "g_can_reply_drop_count", "uwTick"]


def run(command):
    result = subprocess.run(command, cwd=ROOT, capture_output=True)
    output = result.stdout.decode("utf-8", errors="replace")
    if result.returncode or "Error:" in output:
        raise RuntimeError(output + result.stderr.decode(errors="replace"))
    return output


def get_symbols(elf):
    output = run(["arm-none-eabi-nm", "-n", str(elf)])
    found = {}
    for line in output.splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[2] in SYMBOLS:
            found[fields[2]] = int(fields[0], 16)
    if set(found) != set(SYMBOLS):
        raise RuntimeError(f"Missing ELF symbols: {set(SYMBOLS) - set(found)}")
    return found


def snapshot(symbols, directory, name):
    start = min(symbols.values()) & ~3
    size = max(symbols.values()) + 4 - start
    memory_file = directory / (name + ".bin")
    output = run(["STM32_Programmer_CLI", "-c", "port=SWD", "freq=1000", "mode=HotPlug",
                  "-u", hex(start), str(size), str(memory_file),
                  "-r32", "0x40006400", "32", "-score"])
    (directory / (name + ".log")).write_text(output, encoding="utf-8")
    data = memory_file.read_bytes()
    result = {key: (data[address - start] if key == "g_led_cmd" else
                    struct.unpack_from("<I", data, address - start)[0])
              for key, address in symbols.items()}
    for line in output.splitlines():
        if re.match(r"0x40006410\s*:", line):
            result["can_esr"] = int(line.split(":")[1].split()[2], 16)
    result["core_running"] = "Core is running" in output
    if "can_esr" not in result:
        raise RuntimeError("CAN ESR missing from snapshot")
    return result


def exchange(box, payload, duration=0.025):
    accepted = box.send(payload, can_id=0x600)
    replies = box.receive(duration)
    matching = [f for f in replies if f["id"] == "0x601" and not f["remote"] and
                not f["extended"] and f["data"] == payload.hex(" ")]
    return {"request": payload.hex(" "), "accepted": accepted, "replies": replies,
            "passed": accepted == 1 and len(replies) == 1 and len(matching) == 1}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, default=ROOT / "artifacts/can-bringup/F072_CAN_1M.elf")
    parser.add_argument("--count", type=int, default=1000)
    parser.add_argument("--channel", type=int, choices=[0, 1], default=0)
    parser.add_argument("--output", type=Path, default=ROOT / "artifacts/can-bringup")
    args = parser.parse_args()
    directory = args.output.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    # Resolve SRAM only after proving this exact firmware is on the MCU.
    expected_file = directory / "acceptance_expected_flash.bin"
    actual_file = directory / "acceptance_readback_flash.bin"
    run(["arm-none-eabi-objcopy", "-O", "binary", str(args.elf), str(expected_file)])
    expected = expected_file.read_bytes()
    flash_log = run(["STM32_Programmer_CLI", "-c", "port=SWD", "freq=1000", "mode=HotPlug",
                     "-u", "0x08000000", str(len(expected)), str(actual_file)])
    (directory / "acceptance_flash_match.log").write_text(flash_log, encoding="utf-8")
    if actual_file.read_bytes() != expected:
        raise RuntimeError("Board firmware does not match the acceptance ELF; no CAN commands sent")
    symbols = get_symbols(args.elf)
    report = {"elf": str(args.elf), "symbols": {k: hex(v) for k, v in symbols.items()},
              "bitrate": 1000000, "channel": args.channel}
    report["before"] = snapshot(symbols, directory, "acceptance_before")
    box = CanBox(args.channel, 1000000)
    try:
        report["adapter"] = box.info
        report["dlc_tests"] = [exchange(box, bytes(range(n)), 0.04) for n in range(9)]
        # Invalid command formats and unrelated IDs must not alter the LED state.
        invalid = [(2, b"", False, False), (2, b"\xff", False, False),
                   (2, b"\x03", True, False), (2, b"\x04", False, True),
                   (0x123, b"\x03", False, False),
                   (0x600, b"\x03", True, False), (0x600, b"\x03", False, True)]
        report["ignored_frame_tests"] = []
        for can_id, data, extended, remote in invalid:
            accepted = box.send(data, can_id, extended, remote)
            replies = box.receive(0.04)
            report["ignored_frame_tests"].append({"id": hex(can_id), "data": data.hex(" "),
                "extended": extended, "remote": remote, "accepted": accepted,
                "replies": replies, "passed": accepted == 1 and not replies})
        report["continuous"] = []
        started = time.monotonic()
        for i in range(args.count):
            payload = struct.pack("<II", i, i ^ 0xA55AC33C)
            report["continuous"].append(exchange(box, payload, 0.015))
        report["continuous_seconds"] = round(time.monotonic() - started, 3)
        report["after"] = snapshot(symbols, directory, "acceptance_after")
        before, after = report["before"], report["after"]
        report["counter_delta"] = {k: after[k] - before[k] for k in SYMBOLS if k != "g_led_cmd"}
        count = args.count + 9
        report["counter_checks"] = {
            "all_requests_received": report["counter_delta"]["g_can_rx_count"] == count + len(invalid),
            "all_replies_completed": report["counter_delta"]["g_can_tx_count"] == count,
            "no_can_errors": report["counter_delta"]["g_can_error_count"] == 0,
            "no_queue_drops": report["counter_delta"]["g_can_reply_drop_count"] == 0,
            "no_led_command_side_effect": (after["g_led_cmd"] == before["g_led_cmd"] and
                report["counter_delta"]["g_can_cmd_count"] == 0 and
                report["counter_delta"]["g_led_show_count"] == 0),
            "application_running": after["core_running"] and report["counter_delta"]["uwTick"] > 0,
            "error_counters_zero": after["can_esr"] == 0,
        }
        # Reset with the box still listening, then prove startup needs no debugger writes.
        output = run(["STM32_Programmer_CLI", "-c", "port=SWD", "freq=1000", "mode=HotPlug",
                      "-rst", "-run"])
        (directory / "reset.log").write_text(output, encoding="utf-8")
        time.sleep(0.3)
        report["after_reset_tests"] = [exchange(box, struct.pack("<II", i, 0xF072C8), 0.04)
                                       for i in range(20)]
        report["after_reset"] = snapshot(symbols, directory, "acceptance_after_reset")
        reset = report["after_reset"]
        report["reset_checks"] = {
            "received_20": reset["g_can_rx_count"] == 20,
            "transmitted_20": reset["g_can_tx_count"] == 20,
            "no_errors": reset["g_can_error_count"] == 0 and reset["can_esr"] == 0,
            "no_queue_drops": reset["g_can_reply_drop_count"] == 0,
            "core_running": reset["core_running"],
        }
    finally:
        box.close()
    groups = ["dlc_tests", "ignored_frame_tests", "continuous", "after_reset_tests"]
    report["passed"] = (all(t["passed"] for group in groups for t in report[group]) and
                         all(report["counter_checks"].values()) and all(report["reset_checks"].values()))
    (directory / "acceptance.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps({"passed": report["passed"],
        "tests": {group: {"passed": sum(t["passed"] for t in report[group]),
                          "total": len(report[group])} for group in groups},
        "continuous_seconds": report["continuous_seconds"],
        "counter_delta": report["counter_delta"], "counter_checks": report["counter_checks"],
        "after_reset": report["after_reset"], "reset_checks": report["reset_checks"]}, indent=2))
    raise SystemExit(0 if report["passed"] else 1)


if __name__ == "__main__":
    main()
