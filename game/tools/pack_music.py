"""Pack locally downloaded, converted BGM without distributing loose audio files."""

from pathlib import Path
import argparse
import struct

NAMES = (
    "title-white-crystal",
    "prologue-yuki-no-kioku",
    "forest-mori-ni-hisomu-kage",
    "shrine-shinpiteki-na-wa-no-yoru",
    "ending-yuki-no-owari-ni",
)
MAGIC = b"KWBGM1\0\0"


def encode(data: bytes, index: int) -> bytes:
    state = (0x7A53C18D ^ ((index + 1) * 0x9E3779B9)) & 0xFFFFFFFF
    result = bytearray(data)
    for offset in range(len(result)):
        state ^= (state << 13) & 0xFFFFFFFF
        state ^= state >> 17
        state ^= (state << 5) & 0xFFFFFFFF
        result[offset] ^= state & 0xFF
    return bytes(result)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source_dir", type=Path, help="directory containing the five licensed Ogg tracks")
    parser.add_argument("output", type=Path, help="packed game resource")
    args = parser.parse_args()
    tracks = [(args.source_dir / f"{name}.ogg").read_bytes() for name in NAMES]
    if any(not track.startswith(b"OggS") for track in tracks):
        raise ValueError("All inputs must be Ogg Vorbis files")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("wb") as target:
        target.write(MAGIC)
        target.write(struct.pack("<5I", *(len(track) for track in tracks)))
        for index, track in enumerate(tracks):
            target.write(encode(track, index))


if __name__ == "__main__":
    main()
