#!/usr/bin/env python3

import re
import subprocess
from pathlib import Path


# ============================================================
# Configuration
# ============================================================

PROJECT_ROOT = Path.home() / "bodenklavier"

SALAMANDER_ROOT = (
    PROJECT_ROOT
    / "media"
    / "piano_samples"
    / "SalamanderGrandPianoV3_44.1khz16bit"
)

SFZ_FILE = SALAMANDER_ROOT / "SalamanderGrandPianoV3.sfz"

OUTPUT_DIR = PROJECT_ROOT / "media" / "sounds_piano"

# Vélocité MIDI moyenne
TARGET_VELOCITY = 80

# C4 = MIDI 60
# B5 = MIDI 83
FIRST_MIDI_NOTE = 60
LAST_MIDI_NOTE = 83


# ============================================================
# Noms des notes utilisés par AudioEngine
# ============================================================

NOTE_NAMES = [
    "C", "Cs", "D", "Ds", "E", "F",
    "Fs", "G", "Gs", "A", "As", "B"
]


def midi_to_name(midi_note):
    octave = midi_note // 12 - 1
    note = NOTE_NAMES[midi_note % 12]
    return f"{note}{octave}"


# ============================================================
# Lecture du fichier SFZ
# ============================================================

def parse_regions(sfz_path):
    regions = []

    with open(sfz_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:

            if "<region>" not in line:
                continue

            sample_match = re.search(r"sample=([^\s]+)", line)
            lokey_match = re.search(r"lokey=(\d+)", line)
            hikey_match = re.search(r"hikey=(\d+)", line)
            lovel_match = re.search(r"lovel=(\d+)", line)
            hivel_match = re.search(r"hivel=(\d+)", line)
            pitch_match = re.search(
                r"pitch_keycenter=(\d+)",
                line
            )

            # On ne garde ici que les régions correspondant
            # aux samples principaux du piano.
            if not all([
                sample_match,
                lokey_match,
                hikey_match,
                lovel_match
            ]):
                continue

            sample = sample_match.group(1)
            sample = sample.replace("\\", "/")

            lokey = int(lokey_match.group(1))
            hikey = int(hikey_match.group(1))
            lovel = int(lovel_match.group(1))

            # Si hivel est absent, la limite supérieure
            # est considérée comme 127.
            hivel = (
                int(hivel_match.group(1))
                if hivel_match
                else 127
            )

            # La majorité des régions donnent explicitement
            # pitch_keycenter.
            if pitch_match:
                pitch_keycenter = int(
                    pitch_match.group(1)
                )

            else:
                # Certaines régions Salamander, comme C4,
                # ne fournissent pas pitch_keycenter.
                #
                # Exemple :
                # lokey=59 hikey=61
                # centre = 60 = C4
                pitch_keycenter = round(
                    (lokey + hikey) / 2
                )

            region = {
                "sample": sample,
                "lokey": lokey,
                "hikey": hikey,
                "lovel": lovel,
                "hivel": hivel,
                "pitch_keycenter": pitch_keycenter,
            }

            regions.append(region)

    return regions


# ============================================================
# Recherche de la région correspondant à une note
# ============================================================

def find_region(regions, midi_note, velocity):

    candidates = []

    for region in regions:

        if not (
            region["lokey"]
            <= midi_note
            <= region["hikey"]
        ):
            continue

        if not (
            region["lovel"]
            <= velocity
            <= region["hivel"]
        ):
            continue

        candidates.append(region)

    if not candidates:
        return None

    # En cas de plusieurs possibilités, choisir le sample
    # dont la hauteur originale est la plus proche.
    candidates.sort(
        key=lambda r:
        abs(midi_note - r["pitch_keycenter"])
    )

    return candidates[0]


# ============================================================
# Génération d'un WAV avec FFmpeg
# ============================================================

def generate_sample(region, midi_note, output_file):

    input_file = SALAMANDER_ROOT / region["sample"]

    if not input_file.exists():
        print(
            f"    ERREUR : fichier introuvable : "
            f"{input_file}"
        )
        return False

    source_midi = region["pitch_keycenter"]

    semitones = midi_note - source_midi

    # Rapport fréquentiel correspondant au nombre
    # de demi-tons à transposer.
    ratio = 2 ** (semitones / 12.0)

    print(
        f"    source MIDI : {source_midi}"
        f" | transposition : {semitones:+d}"
        f" demi-ton(s)"
        f" | ratio : {ratio:.6f}"
    )

    # Modification de la hauteur puis retour à 44100 Hz.
    audio_filter = (
        f"asetrate=44100*{ratio},"
        f"aresample=44100"
    )

    command = [
        "ffmpeg",
        "-y",
        "-loglevel",
        "error",
        "-i",
        str(input_file),
        "-af",
        audio_filter,
        "-ar",
        "44100",
        "-acodec",
        "pcm_s16le",
        str(output_file),
    ]

    result = subprocess.run(command)

    return result.returncode == 0


# ============================================================
# Programme principal
# ============================================================

def main():

    print()
    print("==============================================")
    print(" Bodenklavier - Piano Sample Generator")
    print("==============================================")
    print()

    print(f"SFZ      : {SFZ_FILE}")
    print(f"Velocity : {TARGET_VELOCITY}")
    print(f"Output   : {OUTPUT_DIR}")
    print()

    if not SFZ_FILE.exists():
        print("ERREUR : fichier SFZ introuvable.")
        return

    OUTPUT_DIR.mkdir(
        parents=True,
        exist_ok=True
    )

    print("Lecture du fichier SFZ...")

    regions = parse_regions(SFZ_FILE)

    print(f"{len(regions)} régions trouvées.")
    print()

    generated = 0

    for midi_note in range(
        FIRST_MIDI_NOTE,
        LAST_MIDI_NOTE + 1
    ):

        note_name = midi_to_name(midi_note)

        print("----------------------------------------------")
        print(f"{note_name} (MIDI {midi_note})")

        region = find_region(
            regions,
            midi_note,
            TARGET_VELOCITY
        )

        if region is None:
            print(
                "    ERREUR : aucune région trouvée."
            )
            continue

        print(
            f"    sample : {region['sample']}"
        )

        print(
            f"    velocity : "
            f"{region['lovel']}-"
            f"{region['hivel']}"
        )

        output_file = (
            OUTPUT_DIR / f"{note_name}.wav"
        )

        success = generate_sample(
            region,
            midi_note,
            output_file
        )

        if success:
            print(
                f"    -> {output_file.name} OK"
            )
            generated += 1
        else:
            print("    -> ECHEC")

    print()
    print("==============================================")
    print(
        f"Terminé : {generated}/24 samples générés."
    )
    print("==============================================")
    print()


if __name__ == "__main__":
    main()