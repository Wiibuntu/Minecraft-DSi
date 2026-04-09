#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
MUSIC_DIR = ROOT / 'nitrofiles' / 'music'
TRACK_NAME = 'Subwoofer Lullaby'
TRACK_MP3 = MUSIC_DIR / 'Minecraft_Volume_Alpha___3___Subwoofer_Lullaby___C418__128k_.mp3'
TRACK_RAW = MUSIC_DIR / 'subwoofer_lullaby_u8_11025.raw'
TRACK_RATE = 11025

STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37,
    41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190,
    209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749,
    3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493,
    10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086,
    29794, 32767
]
INDEX_TABLE = [-1, -1, -1, -1, 2, 4, 6, 8]


def generate_raw_if_possible() -> bool:
    if not TRACK_MP3.exists():
        if TRACK_RAW.exists():
            print(f'Source MP3 missing, keeping existing raw: {TRACK_RAW}', file=sys.stderr)
            return True
        print(f'Missing source MP3 and raw fallback: {TRACK_MP3}', file=sys.stderr)
        return False
    cmd = [
        'ffmpeg', '-y', '-v', 'error',
        '-i', str(TRACK_MP3),
        '-vn',
        '-ac', '1',
        '-ar', str(TRACK_RATE),
        '-af', f'pan=mono|c0=0.5*c0+0.5*c1,highpass=f=25,lowpass=f=4800,compand=attacks=0:points=-90/-90|-60/-55|-30/-24|-12/-9|0/-2,aresample={TRACK_RATE}:resampler=soxr:precision=28:dither_method=triangular_hp',
        '-f', 'u8',
        str(TRACK_RAW),
    ]
    try:
        subprocess.run(cmd, check=True)
        return True
    except Exception as exc:
        print(f'Warning: failed to generate raw music with ffmpeg: {exc}', file=sys.stderr)
        return TRACK_RAW.exists()


def encode_nds_ima_adpcm_u8(raw_bytes: bytes) -> bytes:
    if not raw_bytes:
        return bytes([0, 0, 0, 0])

    first = ((raw_bytes[0] - 128) << 8)
    if first < -0x7FFF:
        first = -0x7FFF
    if first > 0x7FFF:
        first = 0x7FFF

    predictor = int(first)
    index = 0

    if len(raw_bytes) > 1:
        next_sample = ((raw_bytes[1] - 128) << 8)
        target = abs(next_sample - predictor)
        best_index = 0
        best_error = abs(STEP_TABLE[0] - target)
        for i, step in enumerate(STEP_TABLE):
            err = abs(step - target)
            if err < best_error:
                best_error = err
                best_index = i
        index = best_index

    header = ((index & 0x7F) << 16) | (predictor & 0xFFFF)
    out = bytearray(header.to_bytes(4, 'little'))

    nibbles = []
    for byte in raw_bytes[1:]:
        sample = (byte - 128) << 8
        step = STEP_TABLE[index]
        diff = sample - predictor

        nibble = 0
        if diff < 0:
            nibble |= 8
            diff = -diff

        delta = step >> 3
        if diff >= step:
            nibble |= 4
            diff -= step
            delta += step
        if diff >= (step >> 1):
            nibble |= 2
            diff -= step >> 1
            delta += step >> 1
        if diff >= (step >> 2):
            nibble |= 1
            delta += step >> 2

        if nibble & 8:
            predictor -= delta
            if predictor < -0x7FFF:
                predictor = -0x7FFF
        else:
            predictor += delta
            if predictor > 0x7FFF:
                predictor = 0x7FFF

        index += INDEX_TABLE[nibble & 7]
        if index < 0:
            index = 0
        elif index > 88:
            index = 88

        nibbles.append(nibble & 0xF)

    for i in range(0, len(nibbles), 2):
        lo = nibbles[i]
        hi = nibbles[i + 1] if i + 1 < len(nibbles) else 0
        out.append(lo | (hi << 4))

    return bytes(out)


def write_cpp_array(path: Path, payload: bytes, rate: int) -> None:
    chunks = []
    for i in range(0, len(payload), 12):
        part = ', '.join(f'0x{b:02X}' for b in payload[i:i+12])
        chunks.append(f'    {part}')
    body = ',\n'.join(chunks)

    path.write_text(
        '#include "music_track_pcm.h"\n\n'
        f'const unsigned char gBackgroundMusicData[] = {{\n{body}\n}};\n'
        f'const unsigned int gBackgroundMusicDataSize = {len(payload)}u;\n'
        f'const unsigned int gBackgroundMusicRate = {rate}u;\n'
        'const SoundFormat gBackgroundMusicFormat = SoundFormat_ADPCM;\n'
        'const bool gBackgroundMusicLoop = true;\n'
        'const unsigned int gBackgroundMusicLoopPoint = 4u;\n',
        encoding='utf-8',
    )


have_raw = generate_raw_if_possible()

(ROOT / 'include' / 'music_data.h').write_text(
    '#pragma once\n'
    'const char* getMusicTrackPath(int index);\n'
    'const char* getMusicTrackName(int index);\n'
    'int getMusicTrackCount();\n'
    'unsigned int getMusicTrackRate(int index);\n',
    encoding='utf-8',
)

(ROOT / 'source' / 'music_data.cpp').write_text(
    '#include "music_data.h"\n\n'
    'int getMusicTrackCount() { return 1; }\n'
    'const char* getMusicTrackPath(int index) { (void)index; return nullptr; }\n'
    f'const char* getMusicTrackName(int index) {{ return index == 0 ? "{TRACK_NAME}" : "NO MUSIC"; }}\n'
    f'unsigned int getMusicTrackRate(int index) {{ return index == 0 ? {TRACK_RATE}u : 0u; }}\n',
    encoding='utf-8',
)

(ROOT / 'include' / 'music_track_pcm.h').write_text(
    '#pragma once\n'
    '#include <nds.h>\n\n'
    'extern const unsigned char gBackgroundMusicData[];\n'
    'extern const unsigned int gBackgroundMusicDataSize;\n'
    'extern const unsigned int gBackgroundMusicRate;\n'
    'extern const SoundFormat gBackgroundMusicFormat;\n'
    'extern const bool gBackgroundMusicLoop;\n'
    'extern const unsigned int gBackgroundMusicLoopPoint;\n',
    encoding='utf-8',
)

if have_raw and TRACK_RAW.exists():
    raw_bytes = TRACK_RAW.read_bytes()
    adpcm_bytes = encode_nds_ima_adpcm_u8(raw_bytes)
    write_cpp_array(ROOT / 'source' / 'music_track_pcm.cpp', adpcm_bytes, TRACK_RATE)
    print(f'Embedded ADPCM music generated: {len(raw_bytes)} raw -> {len(adpcm_bytes)} ADPCM bytes')
else:
    (ROOT / 'source' / 'music_track_pcm.cpp').write_text(
        '#include "music_track_pcm.h"\n\n'
        'const unsigned char gBackgroundMusicData[] = { 0x00, 0x00, 0x00, 0x80 };\n'
        'const unsigned int gBackgroundMusicDataSize = 4u;\n'
        'const unsigned int gBackgroundMusicRate = 8000u;\n'
        'const SoundFormat gBackgroundMusicFormat = SoundFormat_ADPCM;\n'
        'const bool gBackgroundMusicLoop = true;\n'
        'const unsigned int gBackgroundMusicLoopPoint = 4u;\n',
        encoding='utf-8',
    )
    print('Embedded ADPCM music unavailable; wrote silent fallback.')
