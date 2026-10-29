"""Exercise the dedicated one/three-LED firmware, checking CAN and DMA data.

This script verifies flash provenance before sending any command. It never
sends 03/04 to the normal full-logo firmware. A full 256-pixel PWM buffer is
decoded after each update to confirm all other LEDs are commanded off.
"""
import argparse
import json
import struct
import time
from pathlib import Path

from can_box import CanBox
from verify_can import ROOT, get_symbols, run, snapshot


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hold", type=float, default=4.0)
    args = parser.parse_args()
    folder = ROOT / "artifacts/led-low-load"
    elf = folder / "F072_CAN_1LED_3LED.elf"
    binary = folder / "F072_CAN_1LED_3LED.bin"
    expected = binary.read_bytes()
    log = run(["STM32_Programmer_CLI", "-c", "port=SWD", "freq=1000", "mode=HotPlug",
               "-u", "0x08000000", str(len(expected)), str(folder / "flash_readback.bin")])
    (folder / "flash_match.log").write_text(log, encoding="utf-8")
    if (folder / "flash_readback.bin").read_bytes() != expected:
        raise RuntimeError("Low-load firmware is not on the MCU; no LED commands sent")
    symbols = get_symbols(elf)
    private = {}
    for line in run(["arm-none-eabi-nm", "-n", str(elf)]).splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[2] in ["s_pwm_buffer", "g_led_test_lit_count",
                "g_led_low_load_mode", "g_ws2812_complete_count"]:
            private[fields[2]] = int(fields[0], 16)
    if len(private) != 4:
        raise RuntimeError("Required low-load firmware symbols missing")
    symbols.update({key: value for key, value in private.items() if key != "s_pwm_buffer"})
    report = {"flash_match": True, "before": snapshot(symbols, folder, "before"), "stages": []}
    if report["before"]["g_led_low_load_mode"] != 1:
        raise RuntimeError("Low-load mode has not started")
    box = CanBox(0, 1000000)
    try:
        print(json.dumps({"before": report["before"]}), flush=True)
        stages = [("OFF", 0, 0, (0, 0, 0)), ("ONE_RED", 3, 1, (16, 0, 0)),
                  ("OFF", 0, 0, (0, 0, 0)), ("THREE_BLUE", 4, 3, (0, 0, 16)),
                  ("OFF", 0, 0, (0, 0, 0))]
        for index, (name, command, lit_count, rgb) in enumerate(stages):
            stage = {"name": name, "command": command, "expected_lit_count": lit_count,
                     "accepted": box.send(bytes([command]), can_id=2)}
            print(json.dumps({"sending": stage}), flush=True)
            time.sleep(0.1)
            payload = bytes([0xD3, command, index, 1, 3, 16, 0x72, 0xC8])
            stage["diagnostic_accepted"] = box.send(payload, can_id=0x600)
            stage["replies"] = box.receive(0.15)
            stage["roundtrip_ok"] = any(f["id"] == "0x601" and not f["extended"] and
                not f["remote"] and f["data"] == payload.hex(" ") for f in stage["replies"])
            stage["mcu"] = snapshot(symbols, folder, f"stage_{index}_{name.lower()}")
            pwm_file = folder / f"stage_{index}_pwm.bin"
            run(["STM32_Programmer_CLI", "-c", "port=SWD", "freq=1000", "mode=HotPlug",
                 "-u", hex(private["s_pwm_buffer"]), str((256 * 24 + 80) * 2), str(pwm_file)])
            pwm = struct.unpack("<6224H", pwm_file.read_bytes())
            pixels = []
            for pixel in range(256):
                values = []
                for byte in range(3):
                    value = 0
                    for duty in pwm[pixel * 24 + byte * 8:pixel * 24 + byte * 8 + 8]:
                        if duty not in (17, 34):
                            raise RuntimeError(f"Invalid PWM duty: {duty}")
                        value = (value << 1) | int(duty == 34)
                    values.append(value)
                pixels.append((values[1], values[0], values[2]))
            stage["encoded_lit_count"] = sum(any(p) for p in pixels)
            stage["full_buffer_matches"] = (all(p == (rgb if i < lit_count else (0, 0, 0))
                for i, p in enumerate(pixels)) and all(p == 0 for p in pwm[256 * 24:]))
            previous = report["stages"][-1]["mcu"] if report["stages"] else report["before"]
            stage["counter_delta"] = {key: stage["mcu"][key] - previous[key] for key in
                ["g_can_rx_count", "g_can_cmd_count", "g_led_show_count", "g_ws2812_complete_count", "g_can_error_count"]}
            changed = previous["g_led_cmd"] != command
            stage["passed"] = (stage["accepted"] == 1 and stage["roundtrip_ok"] and
                stage["full_buffer_matches"] and stage["mcu"]["g_led_test_lit_count"] == lit_count and
                stage["counter_delta"]["g_can_rx_count"] == 2 and
                stage["counter_delta"]["g_can_cmd_count"] == 1 and
                stage["counter_delta"]["g_led_show_count"] == int(changed) and
                stage["counter_delta"]["g_ws2812_complete_count"] == int(changed) and
                stage["counter_delta"]["g_can_error_count"] == 0 and stage["mcu"]["can_esr"] == 0)
            report["stages"].append(stage)
            print(json.dumps(stage), flush=True)
            time.sleep(args.hold if lit_count else 1)
        report["passed"] = all(s["passed"] for s in report["stages"])
        print(json.dumps({"passed": report["passed"], "final_state": "OFF"}), flush=True)
    finally:
        box.close()
        (folder / "led_test.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    raise SystemExit(0 if report["passed"] else 1)


if __name__ == "__main__":
    main()
