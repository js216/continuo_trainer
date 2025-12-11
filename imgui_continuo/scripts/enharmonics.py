#!/usr/bin/env python3
import sys

# order of sharps and flats in key signatures
ORDER_SHARPS = ["F","C","G","D","A","E","B"]
ORDER_FLATS  = ["B","E","A","D","G","C","F"]

# key signature map (major)
KEY_SIGS = {
    "C":0, "G":1, "D":2, "A":3, "E":4, "B":5, "F#":6, "C#":7,
    "F":-1, "Bb":-2, "Eb":-3, "Ab":-4, "Db":-5, "Gb":-6, "Cb":-7
}

# the 21 enharmonics in the order
# C C# Db D D# Eb E Fb E# F F# Gb G G# Ab A A# Bb B B# Cb
ENH = [
    ("C",0), ("C",1), ("D",-1),
    ("D",0), ("D",1), ("E",-1),
    ("E",0), ("F",-1), ("E",1),
    ("F",0), ("F",1), ("G",-1),
    ("G",0), ("G",1), ("A",-1),
    ("A",0), ("A",1), ("B",-1),
    ("B",0), ("C",-1), ("B",1)
]

def accidental_needed(step, alt, key_sig):
    """Return 0=none,1=sharp,2=flat,3=natural for spelling step+alt in this key."""
    sig = 0
    # does key signature apply an accidental to this step?
    if key_sig > 0:
        if step in ORDER_SHARPS[:key_sig]:
            sig = 1
    elif key_sig < 0:
        if step in ORDER_FLATS[: -key_sig]:
            sig = -1

    # if key signature already gives us the correct accidental
    if sig == alt:
        return 0

    # need a natural?
    if alt == 0 and sig != 0:
        return 3
    # need sharp?
    if alt == 1:
        return 1
    # need flat?
    if alt == -1:
        return 2

    return 0

def build_row(key):
    ks = KEY_SIGS[key]
    return [accidental_needed(step,alt,ks) for (step,alt) in ENH]

def main():
    keys_in_order = [
        "C","G","D","A","E","B","F#","C#",
        "F","Bb","Eb","Ab","Db","Gb","Cb"
    ]

    for k in keys_in_order:
        row = build_row(k)
        nums = ",".join(str(x) for x in row)
        print(f"       {{{nums}}}, // {k}")

if __name__ == "__main__":
    main()

