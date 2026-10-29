"""CANALYST-II / ControlCAN host test; uses the vendor's 64-bit DLL.

Single-shot CAN transmissions avoid endless retries on a disconnected bus.
VCI_Transmit success only means the adapter accepted a frame. A matching
0x601 reply is required to prove a round trip through the STM32 firmware.
"""
import argparse
import ctypes as C
import json
import time
from pathlib import Path


class InitConfig(C.Structure):
    _fields_ = [("AccCode", C.c_uint32), ("AccMask", C.c_uint32),
                ("Reserved", C.c_uint32), ("Filter", C.c_uint8),
                ("Timing0", C.c_uint8), ("Timing1", C.c_uint8),
                ("Mode", C.c_uint8)]


class Frame(C.Structure):
    _fields_ = [("ID", C.c_uint32), ("TimeStamp", C.c_uint32),
                ("TimeFlag", C.c_uint8), ("SendType", C.c_uint8),
                ("RemoteFlag", C.c_uint8), ("ExternFlag", C.c_uint8),
                ("DataLen", C.c_uint8), ("Data", C.c_uint8 * 8),
                ("Reserved", C.c_uint8 * 3)]


class BoardInfo(C.Structure):
    _fields_ = [("hw_Version", C.c_uint16), ("fw_Version", C.c_uint16),
                ("dr_Version", C.c_uint16), ("in_Version", C.c_uint16),
                ("irq_Num", C.c_uint16), ("can_Num", C.c_uint8),
                ("serial", C.c_char * 20), ("hardware", C.c_char * 40),
                ("Reserved", C.c_uint16 * 4)]


TIMING = {1000000: (0x00, 0x14), 500000: (0x00, 0x1C),
          250000: (0x01, 0x1C), 200000: (0x81, 0xFA),
          125000: (0x03, 0x1C)}


class CanBox:
    def __init__(self, channel=0, bitrate=1000000, silent=False):
        paths = list((Path(__file__).parent / "can_vendor").rglob("ControlCAN.dll"))
        dll = next(p for p in paths if p.parent.name == "x64")
        self.dll = C.WinDLL(str(dll.resolve()))
        self.channel = channel
        self.opened = False
        uint, ptr = C.c_uint32, C.c_void_p
        signatures = {
            "VCI_OpenDevice": [uint, uint, uint],
            "VCI_CloseDevice": [uint, uint],
            "VCI_ReadBoardInfo": [uint, uint, ptr],
            "VCI_InitCAN": [uint, uint, uint, ptr],
            "VCI_StartCAN": [uint, uint, uint],
            "VCI_ResetCAN": [uint, uint, uint],
            "VCI_ClearBuffer": [uint, uint, uint],
            "VCI_Transmit": [uint, uint, uint, ptr, uint],
            "VCI_Receive": [uint, uint, uint, ptr, uint, C.c_int32],
        }
        for name, args in signatures.items():
            fn = getattr(self.dll, name)
            fn.argtypes, fn.restype = args, uint
        if self.dll.VCI_OpenDevice(4, 0, 0) != 1:
            raise RuntimeError("Cannot open CAN adapter (close other CAN applications).")
        self.opened = True
        try:
            info = BoardInfo()
            if self.dll.VCI_ReadBoardInfo(4, 0, C.byref(info)) != 1:
                raise RuntimeError("VCI_ReadBoardInfo failed")
            self.info = {"hardware": info.hardware.decode(errors="replace"),
                         "serial": info.serial.decode(errors="replace"),
                         "channels": info.can_Num, "firmware": hex(info.fw_Version)}
            if channel >= info.can_Num:
                raise ValueError("Adapter does not have the selected CAN channel")
            t0, t1 = TIMING[bitrate]
            config = InitConfig(0, 0xFFFFFFFF, 0, 1, t0, t1, int(silent))
            if self.dll.VCI_InitCAN(4, 0, channel, C.byref(config)) != 1:
                raise RuntimeError("VCI_InitCAN failed")
            if self.dll.VCI_StartCAN(4, 0, channel) != 1:
                raise RuntimeError("VCI_StartCAN failed")
            self.dll.VCI_ClearBuffer(4, 0, channel)
        except BaseException:
            self.close()
            raise

    def close(self):
        if self.opened:
            self.dll.VCI_ResetCAN(4, 0, self.channel)
            self.dll.VCI_CloseDevice(4, 0)
            self.opened = False

    def send(self, data, can_id=2, extended=False, remote=False):
        if len(data) > 8:
            raise ValueError("Classical CAN payload exceeds eight bytes")
        frame = Frame()
        frame.ID, frame.SendType = can_id, 1
        frame.ExternFlag, frame.RemoteFlag = int(extended), int(remote)
        frame.DataLen = len(data)
        frame.Data[:len(data)] = data
        return self.dll.VCI_Transmit(4, 0, self.channel, C.byref(frame), 1)

    def receive(self, duration=0.1):
        frames = []
        deadline = time.monotonic() + duration
        buf = (Frame * 256)()
        while time.monotonic() < deadline:
            n = self.dll.VCI_Receive(4, 0, self.channel, buf, len(buf), 0)
            if n == 0xFFFFFFFF or n > len(buf):
                raise RuntimeError(f"VCI_Receive failed: {n}")
            for f in buf[:n]:
                frames.append({"id": hex(f.ID), "data": bytes(f.Data[:f.DataLen]).hex(" "),
                               "extended": bool(f.ExternFlag), "remote": bool(f.RemoteFlag),
                               "timestamp": f.TimeStamp})
            time.sleep(0.002)
        return frames


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--channel", type=int, choices=[0, 1], default=0,
                        help="0 = box CAN1; 1 = box CAN2")
    parser.add_argument("--bitrate", type=int, choices=TIMING, default=1000000)
    parser.add_argument("--data", default="03", help="Hex bytes, e.g. '03 11 22'")
    parser.add_argument("--count", type=int, default=1)
    parser.add_argument("--interval", type=float, default=0.05)
    parser.add_argument("--listen", type=float, default=0)
    parser.add_argument("--roundtrip", action="store_true")
    parser.add_argument("--exercise-led", action="store_true",
                        help="Also send alternating red/blue commands before diagnostic requests")
    parser.add_argument("--log", type=Path)
    args = parser.parse_args()
    box = CanBox(args.channel, args.bitrate)
    result = {"adapter": box.info, "channel": args.channel, "bitrate": args.bitrate,
              "submitted": 0, "matched": 0, "failed": [], "received": []}
    try:
        if args.listen:
            result["received"] = box.receive(args.listen)
        else:
            for i in range(args.count):
                payload = (bytes([3 + (i % 2)]) + i.to_bytes(4, "little") + b"\xA5\x5A\xC3"
                           if args.roundtrip else bytes.fromhex(args.data))
                if args.exercise_led:
                    if box.send(payload[:1], can_id=2) != 1:
                        raise RuntimeError("LED command submission failed")
                    time.sleep(0.01)
                sent = box.send(payload, can_id=0x600 if args.roundtrip else 2)
                result["submitted"] += int(sent == 1)
                frames = box.receive(args.interval)
                matches = [f for f in frames if f["id"] == "0x601" and
                           f["data"] == payload.hex(" ") and not f["extended"] and not f["remote"]]
                result["received"].extend(frames)
                if len(matches) == 1:
                    result["matched"] += 1
                elif args.roundtrip:
                    result["failed"].append({"sequence": i, "accepted": sent, "received": frames})
        result["passed"] = (result["matched"] == args.count and result["submitted"] == args.count
                            if args.roundtrip else None)
    finally:
        box.close()
    output = json.dumps(result, ensure_ascii=True, indent=2)
    if args.log:
        args.log.parent.mkdir(parents=True, exist_ok=True)
        args.log.write_text(output, encoding="utf-8")
    print(json.dumps({**result, "received": result["received"][:12],
                      "failed": result["failed"][:12]}, indent=2))
    if args.roundtrip and not result["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
