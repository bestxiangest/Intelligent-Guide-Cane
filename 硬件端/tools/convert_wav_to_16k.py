#!/usr/bin/env python3
"""Convert local prompt WAV files to 16 kHz / 16-bit / mono PCM WAV.

Default:
  python tools/convert_wav_to_16k.py

This reads WAV files from ../data/audio and writes converted files to
../data/audio_16k. It never overwrites the source files unless --replace is
provided.
"""

from __future__ import annotations

import argparse
import math
import shutil
import struct
import sys
import wave
from pathlib import Path


TARGET_RATE = 16_000
TARGET_CHANNELS = 1
TARGET_SAMPLE_WIDTH = 2


def clamp_i16(value: float) -> int:
    if value > 32767:
        return 32767
    if value < -32768:
        return -32768
    return int(round(value))


def bytes_to_i16_mono(frames: bytes, channels: int, sample_width: int) -> list[int]:
    if sample_width != 2:
        raise ValueError(f"仅支持16-bit PCM WAV，当前 sample_width={sample_width}")

    values = struct.unpack("<" + "h" * (len(frames) // 2), frames)
    if channels == 1:
        return list(values)

    mono: list[int] = []
    for index in range(0, len(values), channels):
        chunk = values[index : index + channels]
        mono.append(clamp_i16(sum(chunk) / len(chunk)))
    return mono


def resample_linear(samples: list[int], src_rate: int, dst_rate: int) -> list[int]:
    if src_rate == dst_rate:
        return samples[:]
    if not samples:
        return []
    if len(samples) == 1:
        return samples[:]

    dst_len = max(1, int(round(len(samples) * dst_rate / src_rate)))
    ratio = src_rate / dst_rate
    out: list[int] = []

    for i in range(dst_len):
        pos = i * ratio
        left = int(math.floor(pos))
        frac = pos - left
        if left >= len(samples) - 1:
            out.append(samples[-1])
            continue
        value = samples[left] * (1.0 - frac) + samples[left + 1] * frac
        out.append(clamp_i16(value))

    return out


def i16_to_bytes(samples: list[int]) -> bytes:
    if not samples:
        return b""
    return struct.pack("<" + "h" * len(samples), *samples)


def convert_one(src: Path, dst: Path, target_rate: int) -> tuple[int, int, int]:
    with wave.open(str(src), "rb") as reader:
        channels = reader.getnchannels()
        sample_width = reader.getsampwidth()
        src_rate = reader.getframerate()
        frame_count = reader.getnframes()
        comp_type = reader.getcomptype()
        frames = reader.readframes(frame_count)

    if comp_type != "NONE":
        raise ValueError(f"仅支持未压缩PCM WAV，当前 comptype={comp_type}")

    mono = bytes_to_i16_mono(frames, channels, sample_width)
    converted = resample_linear(mono, src_rate, target_rate)

    dst.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(dst), "wb") as writer:
        writer.setnchannels(TARGET_CHANNELS)
        writer.setsampwidth(TARGET_SAMPLE_WIDTH)
        writer.setframerate(target_rate)
        writer.writeframes(i16_to_bytes(converted))

    return src_rate, channels, len(converted)


def default_paths() -> tuple[Path, Path]:
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent
    return project_dir / "data" / "audio", project_dir / "data" / "audio_16k"


def parse_args() -> argparse.Namespace:
    src_default, dst_default = default_paths()
    parser = argparse.ArgumentParser(
        description="批量转换WAV为16kHz/16-bit/mono PCM WAV。"
    )
    parser.add_argument("--src", type=Path, default=src_default, help="源WAV目录")
    parser.add_argument("--dst", type=Path, default=dst_default, help="输出目录")
    parser.add_argument("--rate", type=int, default=TARGET_RATE, help="目标采样率")
    parser.add_argument(
        "--replace",
        action="store_true",
        help="转换完成后用输出文件替换源文件，并把原文件备份到 --backup-dir",
    )
    parser.add_argument(
        "--backup-dir",
        type=Path,
        default=None,
        help="--replace 时保存原文件的目录，默认 data/audio_backup_24k",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    src_dir: Path = args.src.resolve()
    dst_dir: Path = args.dst.resolve()

    if not src_dir.exists():
        print(f"[ERROR] 源目录不存在: {src_dir}", file=sys.stderr)
        return 1

    wav_files = sorted(src_dir.glob("*.wav"))
    if not wav_files:
        print(f"[WARN] 没有找到WAV文件: {src_dir}")
        return 0

    failed = 0
    for src in wav_files:
        dst = dst_dir / src.name
        try:
            src_rate, channels, samples = convert_one(src, dst, args.rate)
            print(
                f"[OK] {src.name}: {src_rate}Hz/{channels}ch -> "
                f"{args.rate}Hz/mono, samples={samples}"
            )
        except Exception as exc:  # noqa: BLE001 - batch converter should continue.
            failed += 1
            print(f"[FAIL] {src.name}: {exc}", file=sys.stderr)

    if failed:
        print(f"[DONE] 完成，但有 {failed} 个文件失败。", file=sys.stderr)
        return 1

    if args.replace:
        backup_dir = (
            args.backup_dir.resolve()
            if args.backup_dir
            else src_dir.parent / "audio_backup_24k"
        )
        backup_dir.mkdir(parents=True, exist_ok=True)
        for converted in sorted(dst_dir.glob("*.wav")):
            original = src_dir / converted.name
            backup = backup_dir / converted.name
            if original.exists():
                shutil.copy2(original, backup)
            shutil.copy2(converted, original)
        print(f"[REPLACE] 已替换源文件，原文件备份在: {backup_dir}")
    else:
        print(f"[DONE] 转换完成，输出目录: {dst_dir}")
        print("[TIP] 确认效果后，可加 --replace 替换源文件。")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
