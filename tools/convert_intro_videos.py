#!/usr/bin/env python3
"""Extract JA2 Smacker intro files from Intro.slf and convert them to MOV."""

import argparse
import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path


VIDEO_NAMES = ("REBEL_CR.SMK", "OMERTA.SMK", "PRAGUE_CR.SMK", "PRAGUE.SMK")
CONVERSION_VERSION = "mpeg1-640x480-25fps-1150k-mp2-128k-44100-v1"


def read_u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def find_entries(archive: Path) -> dict[str, tuple[int, int]]:
    archive_size = archive.stat().st_size
    with archive.open("rb") as stream:
        header = stream.read(532)
        if len(header) != 532:
            raise RuntimeError("Intro.slf header is truncated")
        count = read_u32(header, 512)
        directory_offset = archive_size - count * 280
        if count == 0 or count > 65535 or directory_offset < 532:
            raise RuntimeError(f"invalid Intro.slf directory count={count}")
        stream.seek(directory_offset)
        entries = {}
        for _ in range(count):
            entry = stream.read(280)
            if len(entry) != 280:
                raise RuntimeError("Intro.slf directory is truncated")
            if entry[264] != 0:
                continue
            name = entry[:256].split(b"\0", 1)[0].decode("latin-1").upper()
            if name in VIDEO_NAMES:
                offset = read_u32(entry, 256)
                length = read_u32(entry, 260)
                if offset + length > archive_size:
                    raise RuntimeError(f"entry outside Intro.slf: {name}")
                entries[name] = (offset, length)
    missing = [name for name in VIDEO_NAMES if name not in entries]
    if missing:
        raise RuntimeError("missing Intro.slf entries: " + ", ".join(missing))
    return entries


def convert(archive: Path, cache_dir: Path, output_dir: Path, ffmpeg: str) -> None:
    entries = find_entries(archive)
    cache_dir.mkdir(parents=True, exist_ok=True)
    output_dir.mkdir(parents=True, exist_ok=True)
    version_file = cache_dir / "conversion-version.txt"
    stale_version_file = output_dir / "conversion-version.txt"
    if stale_version_file.exists():
        print(f"removing stale intro video metadata: {stale_version_file}")
        stale_version_file.unlink()
    if not version_file.exists() or version_file.read_text().strip() != CONVERSION_VERSION:
        for stale_movie in output_dir.glob("*.mpg"):
            print(f"removing stale intro video cache: {stale_movie}")
            stale_movie.unlink()
    with archive.open("rb") as source:
        for name in VIDEO_NAMES:
            stem = Path(name).stem
            smk = cache_dir / name
            movie = output_dir / f"{stem}.mpg"
            offset, length = entries[name]
            if movie.exists() and movie.stat().st_mtime_ns >= archive.stat().st_mtime_ns:
                print(f"intro video cache hit: {name} -> {movie}")
                continue
            source.seek(offset)
            payload = source.read(length)
            if len(payload) != length or payload[:4] not in (b"SMK2", b"SMK4"):
                raise RuntimeError(f"invalid Smacker payload: {name}")
            smk.write_bytes(payload)
            print(f"intro video extract: {name} offset={offset} length={length}")
            temporary = movie.with_suffix(".tmp.mpg")
            command = [
                ffmpeg,
                "-y",
                "-hide_banner",
                "-loglevel",
                "warning",
                "-i",
                str(smk),
                "-map",
                "0:v:0",
                "-map",
                "0:a?",
                "-vf",
                "scale=640:480",
                "-r",
                "25",
                "-threads",
                "1",
                "-c:v",
                "mpeg1video",
                "-b:v",
                "1150k",
                "-c:a",
                "mp2",
                "-b:a",
                "128k",
                "-ar",
                "44100",
                "-f",
                "mpeg",
                str(temporary),
            ]
            print("intro video convert: " + " ".join(command))
            try:
                subprocess.run(command, check=True)
                temporary.replace(movie)
            finally:
                temporary.unlink(missing_ok=True)
            print(f"intro video converted: {movie} size={movie.stat().st_size}")
    version_file.write_text(CONVERSION_VERSION + "\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--cache-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--ffmpeg", default="ffmpeg")
    args = parser.parse_args()
    try:
        convert(args.archive, args.cache_dir, args.output_dir, args.ffmpeg)
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"error: intro video conversion failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
