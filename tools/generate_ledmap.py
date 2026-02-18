"""
Generate ledmap.bin from KiCAD PCB file.

Extracts WS2812B footprint positions from kwal25.kicad_pcb,
centers them on the PCB origin, flips Y axis, and writes
as binary float32 pairs sorted by reference designator (D1-D160).

Usage:
    python tools/generate_ledmap.py
    python tools/generate_ledmap.py --verify   # also verify against existing bin
"""
import re, struct, sys, os

PCB_PATH = os.path.join("docs", "kwal25.kicad_pcb")
BIN_PATH = os.path.join("sdroot", "ledmap.bin")

# PCB center (dome origin in KiCAD coordinates)
CENTER_X = 99.937
CENTER_Y = 100.001

EXPECTED_LEDS = 160


def extract_leds(pcb_path):
    with open(pcb_path, encoding="utf-8") as f:
        pcb = f.read()

    fp_pattern = r'\(footprint\s+"[^"]*WS2812[^"]*"\s*(.*?)\n\s*\)\s*\n'
    blocks = re.findall(fp_pattern, pcb, re.DOTALL)

    leds = []
    for block in blocks:
        at_match = re.search(r'\(at\s+([\d.\-]+)\s+([\d.\-]+)', block)
        ref_match = re.search(r'\(property\s+"Reference"\s+"(D\d+)"', block)
        if at_match and ref_match:
            x = float(at_match.group(1))
            y = float(at_match.group(2))
            num = int(ref_match.group(1)[1:])
            leds.append((num, x, y))

    leds.sort(key=lambda t: t[0])
    return leds


def generate(leds, bin_path):
    with open(bin_path, "wb") as f:
        for num, kx, ky in leds:
            x = kx - CENTER_X
            y = -(ky - CENTER_Y)  # KiCAD Y-down -> ledmap Y-up
            f.write(struct.pack("ff", x, y))
    print(f"Written {len(leds)} LED positions to {bin_path} ({len(leds) * 8} bytes)")


def verify(leds, bin_path):
    with open(bin_path, "rb") as f:
        max_err = 0.0
        mismatches = 0
        for i, (num, kx, ky) in enumerate(leds):
            bx, by = struct.unpack("ff", f.read(8))
            conv_x = kx - CENTER_X
            conv_y = -(ky - CENTER_Y)
            err = max(abs(conv_x - bx), abs(conv_y - by))
            if err > max_err:
                max_err = err
            if err > 0.002:
                mismatches += 1
                print(f"  MISMATCH LED {i} (D{num}): err={err:.4f}")

    print(f"Max error: {max_err:.6f}")
    if mismatches == 0:
        print("VERIFIED - all positions match!")
    else:
        print(f"FAILED - {mismatches} mismatches")
    return mismatches == 0


if __name__ == "__main__":
    if not os.path.exists(PCB_PATH):
        print(f"ERROR: {PCB_PATH} not found")
        sys.exit(1)

    leds = extract_leds(PCB_PATH)
    print(f"Extracted {len(leds)} WS2812 LEDs from {PCB_PATH}")

    if len(leds) != EXPECTED_LEDS:
        print(f"WARNING: expected {EXPECTED_LEDS}, got {len(leds)}")

    if "--verify" in sys.argv:
        if not os.path.exists(BIN_PATH):
            print(f"ERROR: {BIN_PATH} not found for verification")
            sys.exit(1)
        verify(leds, BIN_PATH)
    else:
        generate(leds, BIN_PATH)
