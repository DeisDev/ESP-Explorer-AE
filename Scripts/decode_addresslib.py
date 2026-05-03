#!/usr/bin/env python3
"""
Address Library Decoder for Fallout 4
=====================================

Decodes Address Library .bin files to find REL::ID to RVA mappings.

The Address Library binary format is:
1. First 8 bytes: uint64_t count (number of ID-offset pairs)
2. Remaining: array of {uint64_t id, uint64_t offset} pairs

Usage:
    python decode_addresslib.py <version-X.X.X.X.bin> [ID1] [ID2] ...

Examples:
    # Decode entire file to text
    python decode_addresslib.py "version-1-11-191-0.bin"
    
    # Look up specific IDs
    python decode_addresslib.py "version-1-11-191-0.bin" 556439 1278889 1395106 1013228
"""

import struct
import sys
import os
from typing import Dict, List, Optional, Tuple


def decode_address_library(filepath: str) -> Dict[int, int]:
    """
    Decode an Address Library .bin file.
    Returns a dict mapping ID -> offset (RVA).
    """
    id_to_offset = {}
    
    with open(filepath, 'rb') as f:
        # Read count (first 8 bytes)
        count_data = f.read(8)
        if len(count_data) < 8:
            raise ValueError("File too small to contain header")
        
        count = struct.unpack('<Q', count_data)[0]
        print(f"Address Library contains {count:,} entries")
        
        # Read ID-offset pairs
        for i in range(count):
            pair_data = f.read(16)
            if len(pair_data) < 16:
                print(f"Warning: Unexpected end of file at entry {i}")
                break
            
            id_val, offset = struct.unpack('<QQ', pair_data)
            id_to_offset[id_val] = offset
    
    return id_to_offset


def find_ids_near_offset(id_to_offset: Dict[int, int], target_offset: int, 
                         range_bytes: int = 0x100) -> List[Tuple[int, int]]:
    """
    Find IDs with offsets near a target offset.
    Useful for finding related IDs when you know an approximate RVA.
    """
    results = []
    for id_val, offset in id_to_offset.items():
        if abs(offset - target_offset) <= range_bytes:
            results.append((id_val, offset))
    return sorted(results, key=lambda x: x[1])


def export_to_text(id_to_offset: Dict[int, int], output_path: str):
    """Export the database to a text file."""
    with open(output_path, 'w') as f:
        f.write("# Address Library Export\n")
        f.write("# Format: ID\tRVA_OFFSET\n")
        f.write("#\n")
        
        # Sort by ID
        for id_val in sorted(id_to_offset.keys()):
            offset = id_to_offset[id_val]
            f.write(f"{id_val:>10}\t{offset:07X}\n")
    
    print(f"Exported {len(id_to_offset)} entries to {output_path}")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        print("\nExpected Address Library file location:")
        print("  Fallout 4\\Data\\F4SE\\Plugins\\version-X-X-X-X.bin")
        print("\nFor runtime 1.11.191.0, look for:")
        print("  version-1-11-191-0.bin")
        return 1
    
    filepath = sys.argv[1]
    
    if not os.path.exists(filepath):
        print(f"Error: File not found: {filepath}")
        print("\nThe Address Library file is usually located at:")
        print("  <Fallout 4>\\Data\\F4SE\\Plugins\\version-X-X-X-X.bin")
        print("\nDownload Address Library from:")
        print("  https://www.nexusmods.com/fallout4/mods/47327")
        return 1
    
    print(f"Decoding: {filepath}")
    print("-" * 60)
    
    try:
        id_to_offset = decode_address_library(filepath)
    except Exception as e:
        print(f"Error decoding file: {e}")
        return 1
    
    # If specific IDs provided, look them up
    if len(sys.argv) > 2:
        print("\nLooking up specific REL::IDs:")
        print("-" * 60)
        
        target_ids = []
        for arg in sys.argv[2:]:
            try:
                target_ids.append(int(arg))
            except ValueError:
                print(f"Warning: '{arg}' is not a valid ID number")
        
        for id_val in target_ids:
            if id_val in id_to_offset:
                offset = id_to_offset[id_val]
                abs_addr = 0x140000000 + offset
                print(f"REL::ID({id_val:>10}):  RVA = 0x{offset:07X}  (0x{abs_addr:X})")
            else:
                print(f"REL::ID({id_val:>10}):  NOT FOUND in database")
        
        # Also look up UneducatedShooter-NG specific offsets
        print("\n" + "=" * 60)
        print("UneducatedShooter-NG Hook Analysis:")
        print("=" * 60)
        
        # RunActorUpdates hook: REL::ID(556439) + 0x17
        if 556439 in id_to_offset:
            base = id_to_offset[556439]
            hook_offset = base + 0x17
            print(f"\nptr_RunActorUpdates:")
            print(f"  REL::ID(556439) = RVA 0x{base:07X}")
            print(f"  + offset 0x17   = RVA 0x{hook_offset:07X}")
            print(f"  Absolute        = 0x{0x140000000 + hook_offset:X}")
        
        # ADS distance check: REL::ID(1278889) + 0x31
        if 1278889 in id_to_offset:
            base = id_to_offset[1278889]
            hook_offset = base + 0x31
            print(f"\nptr_ADS_DistanceCheck:")
            print(f"  REL::ID(1278889) = RVA 0x{base:07X}")
            print(f"  + offset 0x31    = RVA 0x{hook_offset:07X}")
            print(f"  Absolute         = 0x{0x140000000 + hook_offset:X}")
        
        # bhkWorld update: REL::ID(1395106)
        if 1395106 in id_to_offset:
            offset = id_to_offset[1395106]
            print(f"\nptr_bhkWorldUpdate:")
            print(f"  REL::ID(1395106) = RVA 0x{offset:07X}")
            print(f"  Absolute         = 0x{0x140000000 + offset:X}")
        
        # Engine time: REL::ID(1013228)
        if 1013228 in id_to_offset:
            offset = id_to_offset[1013228]
            print(f"\nEngineTime:")
            print(f"  REL::ID(1013228) = RVA 0x{offset:07X}")
            print(f"  Absolute         = 0x{0x140000000 + offset:X}")
    
    else:
        # Export entire database
        output = filepath.replace('.bin', '.txt')
        export_to_text(id_to_offset, output)
        
        # Print some statistics
        print("\nDatabase Statistics:")
        print(f"  Total entries: {len(id_to_offset):,}")
        
        offsets = list(id_to_offset.values())
        if offsets:
            print(f"  Offset range: 0x{min(offsets):07X} - 0x{max(offsets):07X}")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
