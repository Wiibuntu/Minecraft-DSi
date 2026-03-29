#!/usr/bin/env python3
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MUSIC_DIR = ROOT / 'music'
NITRO_MUSIC_DIR = ROOT / 'nitrofiles' / 'music'
INCLUDE_OUT = ROOT / 'include' / 'music_data.h'
SOURCE_OUT = ROOT / 'source' / 'music_data.cpp'
SAMPLE_RATE = 11025


def sanitize(name: str) -> str:
    out = re.sub(r'[^0-9A-Za-z_]', '_', name)
    if not out or out[0].isdigit():
        out = f'track_{out}'
    return out


def convert_audio_to_raw(src: Path, dst: Path) -> None:
    cmd = [
        'ffmpeg', '-y', '-i', str(src),
        '-vn',
        '-f', 'u8',
        '-acodec', 'pcm_u8',
        '-ac', '1',
        '-ar', str(SAMPLE_RATE),
        str(dst),
    ]
    subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)


def write_header() -> None:
    INCLUDE_OUT.write_text(
        "#pragma once\n\n"
        "const char* getMusicTrackPath(int index);\n"
        "const char* getMusicTrackName(int index);\n"
        "int getMusicTrackSampleRate(int index);\n"
        "int getMusicTrackCount();\n",
        encoding='utf-8',
    )


def write_source(entries) -> None:
    lines = [
        '#include "music_data.h"',
        '',
        'namespace {',
        'struct MusicTrackEntry { const char* name; const char* path; int sampleRate; };',
        'static const MusicTrackEntry kTracks[] = {'
    ]
    for entry in entries:
        lines.append(f'    {{"{entry["name"]}", "nitro:/music/{entry["raw_name"]}", {entry["sample_rate"]}}},')
    lines += [
        '};',
        'static const MusicTrackEntry kEmptyTrack = {"NO MUSIC", nullptr, 11025};',
        '} // namespace',
        '',
        'int getMusicTrackCount() {',
        '    return (int)(sizeof(kTracks) / sizeof(kTracks[0]));',
        '}',
        '',
        'const char* getMusicTrackPath(int index) {',
        '    const int count = getMusicTrackCount();',
        '    if (index < 0 || index >= count) return nullptr;',
        '    return kTracks[index].path;',
        '}',
        '',
        'const char* getMusicTrackName(int index) {',
        '    const int count = getMusicTrackCount();',
        '    if (index < 0 || index >= count) return kEmptyTrack.name;',
        '    return kTracks[index].name;',
        '}',
        '',
        'int getMusicTrackSampleRate(int index) {',
        '    const int count = getMusicTrackCount();',
        '    if (index < 0 || index >= count) return kEmptyTrack.sampleRate;',
        '    return kTracks[index].sampleRate;',
        '}',
    ]
    SOURCE_OUT.write_text('\n'.join(lines) + '\n', encoding='utf-8')


def main() -> None:
    MUSIC_DIR.mkdir(exist_ok=True)
    NITRO_MUSIC_DIR.mkdir(parents=True, exist_ok=True)

    sources = []
    for pat in ('*.mp3', '*.MP3', '*.wav', '*.WAV'):
        sources.extend(sorted(MUSIC_DIR.glob(pat)))

    if sources:
        for old in NITRO_MUSIC_DIR.glob('*.raw'):
            old.unlink()
        for old in NITRO_MUSIC_DIR.glob('*.RAW'):
            old.unlink()

    print('Preparing music playlist ...')

    entries = []
    for src in sources:
        raw_name = sanitize(src.stem) + '.raw'
        dst = NITRO_MUSIC_DIR / raw_name
        try:
            convert_audio_to_raw(src, dst)
        except Exception as exc:
            print(f'Skipping {src.name}: {exc}')
            continue
        entries.append({'name': src.stem, 'raw_name': raw_name, 'sample_rate': SAMPLE_RATE})
        print(f'Prepared {src.name} -> nitrofiles/music/{raw_name} @ {SAMPLE_RATE} Hz mono PCM u8')

    if not entries:
        existing_raw = sorted(NITRO_MUSIC_DIR.glob('*.raw')) + sorted(NITRO_MUSIC_DIR.glob('*.RAW'))
        for raw in existing_raw:
            entries.append({'name': raw.stem, 'raw_name': raw.name, 'sample_rate': SAMPLE_RATE})
        if existing_raw:
            print('No source MP3/WAV files found in music/. Reusing existing nitrofiles/music RAW files.')

    write_header()
    write_source(entries)

    if not entries:
        print('No music files found. Add .mp3 or .wav files to music/ and rebuild.')


if __name__ == '__main__':
    main()
