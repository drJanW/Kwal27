"""Verify ledmap.bin matches KiCAD PCB WS2812 positions."""
import re, struct

with open("docs/kwal25.kicad_pcb", encoding="utf-8") as f:
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
print(f"Extracted {len(leds)} WS2812 LEDs from KiCAD PCB")

# PCB center (origin of the dome)
cx, cy = 99.937, 100.001

with open("sdroot/ledmap.bin", "rb") as f:
    max_err = 0.0
    for i, (num, kx, ky) in enumerate(leds):
        bx, by = struct.unpack("ff", f.read(8))
        conv_x = kx - cx
        conv_y = -(ky - cy)   # KiCAD Y is down, ledmap Y is up
        ex = abs(conv_x - bx)
        ey = abs(conv_y - by)
        err = max(ex, ey)
        if err > max_err:
            max_err = err
        if err > 0.002:
            print(f"LED {i:3d} (D{num}): conv=({conv_x:8.3f},{conv_y:8.3f}) bin=({bx:8.3f},{by:8.3f}) err={err:.4f}")

print(f"Max error: {max_err:.6f}")
if max_err < 0.002:
    print("PERFECT MATCH - all 160 LEDs verified!")
