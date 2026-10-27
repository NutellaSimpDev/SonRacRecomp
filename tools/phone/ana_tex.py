"""Analyze the one-texture dump written by vulkan_dump_texture_*.

Usage: py tools/phone/ana_tex.py [dump-directory]
The directory defaults to out/texdump.
"""

from pathlib import Path
import re
import sys

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
DUMP_DIR = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "out" / "texdump"


def read_info(path: Path) -> dict[str, int]:
    fields = {}
    for key, value in re.findall(r"([a-z_]+)=(-?\d+)", path.read_text()):
        fields[key] = int(value)
    required = {"format", "width", "height", "pitch", "tiled", "endian", "bytes_per_host_block"}
    missing = required - fields.keys()
    if missing:
        raise ValueError(f"Missing fields in {path}: {', '.join(sorted(missing))}")
    return fields


def tiled_offset(x: np.ndarray, y: np.ndarray, pitch: int, bytes_per_pixel_log2: int) -> np.ndarray:
    pitch_aligned = (pitch + 31) & ~31
    macro = ((x >> 5) + (y >> 5) * (pitch_aligned >> 5)) << (bytes_per_pixel_log2 + 7)
    micro = ((x & 7) + ((y & 0xE) << 2)) << bytes_per_pixel_log2
    offset = macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 1) << 4)
    return ((offset & ~0x1FF) << 3) + ((y & 16) << 7) + ((offset & 0x1C0) << 2) + \
        (((((y & 8) >> 2) + (x >> 3)) & 3) << 6) + (offset & 0x3F)


def apply_guest_endian(data: np.ndarray, endian: int) -> np.ndarray:
    data = data.copy()
    if endian == 0:
        return data
    if endian == 1:  # 8-in-16
        return data.reshape(*data.shape[:-1], 4, 2)[..., ::-1].reshape(data.shape)
    words = data.reshape(*data.shape[:-1], 2, 4)
    if endian == 2:  # 8-in-32
        return words[..., ::-1].reshape(data.shape)
    if endian == 3:  # 16-in-32
        return words[..., [2, 3, 0, 1]].reshape(data.shape)
    raise ValueError(f"Unsupported Xenos endian mode {endian}")


def image_stats(name: str, pixels: np.ndarray) -> None:
    finite = np.nan_to_num(pixels.astype(np.float32), nan=0.0, posinf=0.0, neginf=0.0)
    print(name, "NaN", int(np.isnan(pixels).sum()), "Inf", int(np.isinf(pixels).sum()))
    print("  RGBA mean", np.round(finite.reshape(-1, 4).mean(0), 4),
          "min", np.round(finite.reshape(-1, 4).min(0), 4),
          "max", np.round(finite.reshape(-1, 4).max(0), 4))
    luminance = finite[..., :3].mean(-1)
    tile_count = luminance.shape[1] // 32
    print("  32px column means", np.round(luminance[:, :tile_count * 32]
                                               .reshape(luminance.shape[0], tile_count, 32)
                                               .mean((0, 2)), 3))
    preview = np.clip(finite[..., :3], 0.0, 1.0) ** (1.0 / 2.2)
    Image.fromarray((preview * 255).astype(np.uint8)).save(DUMP_DIR / f"{name}.png")


def main() -> None:
    info = read_info(DUMP_DIR / "texdump_info.txt")
    width, height = info["width"], info["height"]
    if info["format"] != 32 or info["bytes_per_host_block"] != 8:
        raise ValueError(f"Expected the 64bpp HDR scene texture, got {info}")
    if not info["tiled"]:
        raise ValueError("Expected a tiled Xenos source texture")

    source = np.fromfile(DUMP_DIR / "texdump_source.bin", dtype=np.uint8)
    loaded = np.fromfile(DUMP_DIR / "texdump_loaded.bin", dtype=np.uint8)
    y, x = np.mgrid[0:height, 0:width]
    offsets = tiled_offset(x, y, info["pitch"], 3)
    if int(offsets.max()) + 8 > source.size:
        raise ValueError(f"Source dump too short: need {int(offsets.max()) + 8}, have {source.size}")

    untiled = np.empty((height, width, 8), dtype=np.uint8)
    for byte in range(8):
        untiled[..., byte] = source[offsets + byte]
    untiled = apply_guest_endian(untiled, info["endian"])

    row_pitch = info.get("host_x_pitch_blocks", width) * info["bytes_per_host_block"]
    if row_pitch < width * 8 or row_pitch * height > loaded.size:
        if loaded.size % height:
            raise ValueError(f"Loaded dump size {loaded.size} is not row aligned for {height} rows")
        row_pitch = loaded.size // height
    if row_pitch < width * 8:
        raise ValueError(f"Loaded row pitch {row_pitch} is smaller than {width * 8}")
    loaded_rows = loaded[:row_pitch * height].reshape(height, row_pitch)[:, :width * 8]
    loaded_pixels = loaded_rows.copy().reshape(height, width, 8).view(np.float16).reshape(height, width, 4)
    untiled_pixels = untiled.copy().reshape(height, width, 8).view(np.float16).reshape(height, width, 4)

    different = np.any(untiled != loaded_rows.reshape(height, width, 8), axis=-1)
    print(f"{width}x{height}, pitch={info['pitch']}, endian={info['endian']}, "
          f"loaded row pitch={row_pitch}, source={source.size} bytes, loaded={loaded.size} bytes")
    print(f"CPU untiling differs from Vulkan output at {int(different.sum())} / {width * height} pixels")
    image_stats("loaded", loaded_pixels)
    image_stats("cpu", untiled_pixels)


if __name__ == "__main__":
    main()
