"""Extract the user's official x64 ControlCAN DLL without redistributing it."""
import argparse
import hashlib
import shutil
import subprocess
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", required=True, type=Path)
    args = parser.parse_args()
    archive = args.archive.resolve()
    if not archive.is_file():
        parser.error(f"Archive does not exist: {archive}")
    tar = shutil.which("tar")
    if not tar:
        parser.error("tar is required; Windows 10/11 includes tar.exe")
    output = Path(__file__).resolve().parent / "can_vendor" / "x64"
    with tempfile.TemporaryDirectory(prefix="rlogo-can-") as scratch:
        subprocess.run([tar, "-xf", str(archive), "-C", scratch,
                        "--include", "*x64/ControlCAN.dll",
                        "--include", "*ControlCAN For VC/ControlCAN.h"], check=True)
        libraries = sorted(Path(scratch).rglob("ControlCAN.dll"))
        dll = next((p for p in libraries if p.parent.name.lower() == "x64"), None)
        if dll is None:
            raise SystemExit("No x64/ControlCAN.dll found in this official archive")
        data = dll.read_bytes()
        if data[:2] != b"MZ":
            raise SystemExit("Selected DLL is not a Windows PE file")
        pe = int.from_bytes(data[0x3c:0x40], "little")
        if data[pe:pe+4] != b"PE\0\0" or int.from_bytes(data[pe+4:pe+6], "little") != 0x8664:
            raise SystemExit("Selected ControlCAN DLL is not Windows x64")
        output.mkdir(parents=True, exist_ok=True)
        shutil.copy2(dll, output / "ControlCAN.dll")
        header = next(Path(scratch).rglob("ControlCAN.h"), None)
        if header:
            shutil.copy2(header, output / "ControlCAN.h")
        print(f"Installed local x64 DLL: {output / 'ControlCAN.dll'}")
        print(f"SHA-256: {hashlib.sha256(data).hexdigest()}")


if __name__ == "__main__":
    main()
