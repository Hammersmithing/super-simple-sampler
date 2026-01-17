#!/usr/bin/env python3
"""
Super Simple Sampler - Instrument Builder

Scans a folder of audio samples and generates an instrument.sss XML file.

Naming convention:
    {Note}_{Velocity}_{RR}.wav

Examples:
    C3_033_01.wav      (C3, velocity 33, round robin 1)
    C#3_127_02.wav     (C#3, velocity 127, round robin 2)
    Db3_064_01.wav     (Db3, velocity 64, round robin 1)
    F#4_100_03.wav     (F#4, velocity 100, round robin 3)

Usage:
    python build_instrument.py /path/to/samples --name "My Instrument" --author "Your Name"
    python build_instrument.py /path/to/samples -o /path/to/output/instrument.sss
"""

import os
import re
import argparse
from pathlib import Path
from collections import defaultdict
from typing import Dict, List, Tuple, Optional

# Note name to MIDI number mapping
NOTE_MAP = {
    'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11
}

AUDIO_EXTENSIONS = {'.wav', '.aiff', '.aif', '.flac', '.mp3', '.ogg'}


def note_to_midi(note_str: str) -> Optional[int]:
    """Convert note name (like C#2, Db3, A4) to MIDI note number."""
    match = re.match(r'^([A-Ga-g])([#b]?)(-?\d+)$', note_str)
    if not match:
        return None

    note_letter = match.group(1).upper()
    accidental = match.group(2)
    octave = int(match.group(3))

    midi_note = NOTE_MAP.get(note_letter)
    if midi_note is None:
        return None

    # Apply accidental
    if accidental == '#':
        midi_note += 1
    elif accidental == 'b':
        midi_note -= 1

    # MIDI note calculation: C4 = 60, so C0 = 12
    midi_note += (octave + 1) * 12

    return midi_note


def midi_to_note(midi_num: int) -> str:
    """Convert MIDI note number to note name."""
    note_names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
    octave = (midi_num // 12) - 1
    note = note_names[midi_num % 12]
    return f"{note}{octave}"


def parse_filename(filename: str) -> Optional[Dict]:
    """
    Parse a sample filename.

    Expected format: {Note}_{Velocity}_{RR}_{OptionalSuffix}.ext

    Returns dict with: note, midi_note, velocity, round_robin
    """
    # Remove extension
    name = Path(filename).stem

    # Pattern: Note_Velocity_RR with optional _suffix (e.g., C3_033_01, F#4_127_02_piano)
    pattern = r'^([A-Ga-g][#b]?-?\d+)_(\d+)_(\d+)(?:_.*)?$'
    match = re.match(pattern, name, re.IGNORECASE)

    if not match:
        return None

    note_str = match.group(1)
    velocity = int(match.group(2))
    round_robin = int(match.group(3))

    midi_note = note_to_midi(note_str)
    if midi_note is None:
        return None

    return {
        'note': note_str,
        'midi_note': midi_note,
        'velocity': velocity,
        'round_robin': round_robin,
        'filename': filename
    }


def calculate_velocity_ranges(velocities: List[int]) -> Dict[int, Tuple[int, int]]:
    """
    Calculate velocity ranges for a set of velocity values.

    Returns dict mapping velocity value to (low_vel, high_vel) tuple.
    """
    velocities = sorted(set(velocities))

    if len(velocities) == 1:
        return {velocities[0]: (1, 127)}

    ranges = {}
    for i, vel in enumerate(velocities):
        if i == 0:
            low = 1
        else:
            # Midpoint between this and previous velocity
            low = (velocities[i-1] + vel) // 2 + 1

        if i == len(velocities) - 1:
            high = 127
        else:
            # Midpoint between this and next velocity
            high = (vel + velocities[i+1]) // 2

        ranges[vel] = (low, high)

    return ranges


def calculate_note_ranges(midi_notes: List[int]) -> Dict[int, Tuple[int, int]]:
    """
    Calculate key ranges for a set of root notes.

    Each sample covers from (previous root + 1) up to its own root note.
    This means samples are only pitched DOWN, never up (except the highest
    sample which extends to 127).

    Returns dict mapping midi note to (low_note, high_note) tuple.
    """
    midi_notes = sorted(set(midi_notes))

    if len(midi_notes) == 1:
        return {midi_notes[0]: (0, 127)}

    ranges = {}
    for i, note in enumerate(midi_notes):
        if i == 0:
            low = 0
        else:
            # Start from previous note + 1
            low = midi_notes[i-1] + 1

        if i == len(midi_notes) - 1:
            # Highest sample extends to cover remaining keys
            high = 127
        else:
            # End at this note's root (no pitching up)
            high = note

        ranges[note] = (low, high)

    return ranges


def scan_samples(samples_dir: Path) -> List[Dict]:
    """Scan directory for audio samples and parse their filenames."""
    samples = []

    for file in samples_dir.iterdir():
        if file.is_file() and file.suffix.lower() in AUDIO_EXTENSIONS:
            parsed = parse_filename(file.name)
            if parsed:
                parsed['path'] = str(file.relative_to(samples_dir.parent))
                samples.append(parsed)
            else:
                print(f"  Warning: Could not parse filename: {file.name}")

    return samples


def generate_xml(samples: List[Dict], name: str, author: str, samples_folder: str = "samples") -> str:
    """Generate instrument.sss XML content."""

    if not samples:
        raise ValueError("No valid samples found!")

    # Group samples by note and velocity
    # Structure: {midi_note: {velocity: [samples]}}
    grouped = defaultdict(lambda: defaultdict(list))

    for sample in samples:
        grouped[sample['midi_note']][sample['velocity']].append(sample)

    # Get all unique notes and velocities
    all_notes = sorted(grouped.keys())
    all_velocities = sorted(set(s['velocity'] for s in samples))

    # Calculate ranges
    note_ranges = calculate_note_ranges(all_notes)
    vel_ranges = calculate_velocity_ranges(all_velocities)

    # Build XML
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<SuperSimpleSampler version="1.0">',
        '  <meta>',
        f'    <name>{name}</name>',
        f'    <author>{author}</author>',
        '  </meta>',
        '',
        '  <samples>',
    ]

    # Add comment with stats
    lines.append(f'    <!-- Generated from {len(samples)} sample files -->')
    lines.append(f'    <!-- Notes: {midi_to_note(all_notes[0])} - {midi_to_note(all_notes[-1])} ({len(all_notes)} zones) -->')
    lines.append(f'    <!-- Velocity layers: {len(all_velocities)} ({all_velocities}) -->')
    lines.append('')

    # Generate sample entries
    for midi_note in all_notes:
        note_low, note_high = note_ranges[midi_note]

        for velocity in sorted(grouped[midi_note].keys()):
            vel_low, vel_high = vel_ranges[velocity]

            rr_samples = grouped[midi_note][velocity]
            # Sort by round robin number
            rr_samples.sort(key=lambda x: x['round_robin'])

            # Add comment for this zone
            note_name = midi_to_note(midi_note)
            lines.append(f'    <!-- {note_name} vel{velocity} ({len(rr_samples)} round robins) -->')

            for sample in rr_samples:
                # Path relative to instrument folder
                rel_path = sample['path']

                line = f'    <sample file="{rel_path}" '
                line += f'rootNote="{midi_note}" '
                line += f'loNote="{note_low}" hiNote="{note_high}" '
                line += f'loVel="{vel_low}" hiVel="{vel_high}"/>'
                lines.append(line)

            lines.append('')

    lines.append('  </samples>')
    lines.append('</SuperSimpleSampler>')

    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(
        description='Build a Super Simple Sampler instrument from a folder of samples.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Naming convention:
    {Note}_{Velocity}_{RR}.wav

Examples:
    C3_033_01.wav   (C3, velocity 33, round robin 1)
    C#3_127_02.wav  (C#3, velocity 127, round robin 2)
    Db3_064_01.wav  (Db3, velocity 64, round robin 1)
        """
    )

    parser.add_argument('samples_dir', type=str,
                        help='Path to folder containing sample files')
    parser.add_argument('-n', '--name', type=str, default='My Instrument',
                        help='Instrument name (default: "My Instrument")')
    parser.add_argument('-a', '--author', type=str, default='',
                        help='Author name')
    parser.add_argument('-o', '--output', type=str,
                        help='Output path for instrument.sss (default: samples_dir/../instrument.sss)')

    args = parser.parse_args()

    samples_dir = Path(args.samples_dir).resolve()

    if not samples_dir.exists():
        print(f"Error: Directory not found: {samples_dir}")
        return 1

    if not samples_dir.is_dir():
        print(f"Error: Not a directory: {samples_dir}")
        return 1

    print(f"Scanning: {samples_dir}")
    samples = scan_samples(samples_dir)

    if not samples:
        print("Error: No valid sample files found!")
        print("Expected naming format: {Note}_{Velocity}_{RR}.wav")
        print("Example: C3_033_01.wav or F#4_127_02.wav")
        return 1

    print(f"Found {len(samples)} valid samples")

    # Generate XML
    xml_content = generate_xml(samples, args.name, args.author)

    # Determine output path
    if args.output:
        output_path = Path(args.output)
    else:
        output_path = samples_dir.parent / 'instrument.sss'

    # Write file
    output_path.write_text(xml_content)
    print(f"Generated: {output_path}")

    # Summary
    all_notes = sorted(set(s['midi_note'] for s in samples))
    all_velocities = sorted(set(s['velocity'] for s in samples))

    print(f"\nInstrument Summary:")
    print(f"  Name: {args.name}")
    print(f"  Notes: {midi_to_note(all_notes[0])} - {midi_to_note(all_notes[-1])} ({len(all_notes)} zones)")
    print(f"  Velocity layers: {all_velocities}")
    print(f"  Total samples: {len(samples)}")

    return 0


if __name__ == '__main__':
    exit(main())
