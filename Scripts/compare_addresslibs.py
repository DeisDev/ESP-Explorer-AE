#!/usr/bin/env python3
"""
Address Library Cross-Version Comparator
=========================================

Compares Address Library databases between versions to help find
equivalent functions when IDs changed between game versions.

This tool helps solve the problem where:
- REL::ID(X) exists in version A
- REL::ID(X) does NOT exist in version B
- But the function still exists at a different RVA in version B

Usage:
    python compare_addresslibs.py <old_db.bin> <new_db.bin> <old_id> [--search-range <bytes>]

Examples:
    # Find what function OLD ID 556439 became in the new version
    python compare_addresslibs.py version-1-10-163-0.bin version-1-11-191-0.bin 556439

    # With larger search range for nearby functions
    python compare_addresslibs.py version-1-10-163-0.bin version-1-11-191-0.bin 556439 --search-range 0x1000
"""

import struct
import sys
import os
from typing import Dict, List, Tuple, Optional


def decode_address_library(filepath: str) -> Dict[int, int]:
    """Decode an Address Library .bin file. Returns dict mapping ID -> offset (RVA)."""
    id_to_offset = {}
    
    with open(filepath, 'rb') as f:
        count_data = f.read(8)
        if len(count_data) < 8:
            raise ValueError("File too small")
        
        count = struct.unpack('<Q', count_data)[0]
        
        for _ in range(count):
            pair_data = f.read(16)
            if len(pair_data) < 16:
                break
            id_val, offset = struct.unpack('<QQ', pair_data)
            id_to_offset[id_val] = offset
    
    return id_to_offset


def find_by_rva_delta(old_db: Dict[int, int], new_db: Dict[int, int], 
                       old_id: int, max_delta: int = 0x10000) -> List[Tuple[int, int, int, int]]:
    """
    Find potential new IDs by looking for functions with similar RVA deltas
    to nearby functions that exist in both versions.
    
    Returns list of (new_id, new_rva, old_rva, delta_diff)
    """
    if old_id not in old_db:
        return []
    
    old_rva = old_db[old_id]
    
    # Build reverse lookup for new DB
    new_rva_to_id = {rva: id_val for id_val, rva in new_db.items()}
    
    # Find IDs that exist in BOTH databases
    common_ids = set(old_db.keys()) & set(new_db.keys())
    
    # For each common ID, compute RVA delta between versions
    deltas = []
    for common_id in common_ids:
        delta = new_db[common_id] - old_db[common_id]
        deltas.append(delta)
    
    if not deltas:
        print("No common IDs found between databases!")
        return []
    
    # Find most common deltas (version updates often shift code by similar amounts)
    from collections import Counter
    delta_counts = Counter(deltas)
    common_deltas = delta_counts.most_common(20)
    
    print(f"\nMost common RVA deltas between versions:")
    for delta, count in common_deltas[:10]:
        print(f"  Delta {delta:+#x}: {count} functions")
    
    # Predict where the old function might be in new version
    results = []
    most_likely_delta = common_deltas[0][0]
    predicted_rva = old_rva + most_likely_delta
    
    print(f"\n\nOld REL::ID({old_id}) at RVA 0x{old_rva:X}")
    print(f"Predicted new RVA: 0x{predicted_rva:X} (using delta {most_likely_delta:+#x})")
    
    # Find functions near the predicted RVA
    for new_id, new_rva in new_db.items():
        if abs(new_rva - predicted_rva) <= max_delta:
            delta_diff = new_rva - predicted_rva
            results.append((new_id, new_rva, old_rva, delta_diff))
    
    results.sort(key=lambda x: abs(x[3]))
    return results


def find_by_neighborhood(old_db: Dict[int, int], new_db: Dict[int, int], 
                          old_id: int, neighborhood: int = 0x1000) -> List[Tuple[int, int, int]]:
    """
    Find potential new IDs by looking at neighboring functions.
    
    If functions A, B, C are neighbors in old version and A, C exist in new version,
    we can estimate where B should be.
    """
    if old_id not in old_db:
        return []
    
    old_rva = old_db[old_id]
    
    # Find IDs in old version that are near our target
    old_neighbors = []
    for id_val, rva in old_db.items():
        if id_val != old_id and abs(rva - old_rva) <= neighborhood:
            old_neighbors.append((id_val, rva))
    
    old_neighbors.sort(key=lambda x: x[1])
    
    # Check which neighbors exist in new version
    mapped_neighbors = []
    for neighbor_id, neighbor_old_rva in old_neighbors:
        if neighbor_id in new_db:
            neighbor_new_rva = new_db[neighbor_id]
            mapped_neighbors.append((neighbor_id, neighbor_old_rva, neighbor_new_rva))
    
    if not mapped_neighbors:
        return []
    
    print(f"\nNeighboring functions that exist in both versions:")
    for n_id, n_old, n_new in mapped_neighbors[:10]:
        delta = n_new - n_old
        print(f"  REL::ID({n_id}): old=0x{n_old:X}, new=0x{n_new:X}, delta={delta:+#x}")
    
    # Estimate new RVA based on neighbors
    # Use weighted average based on distance
    weighted_deltas = []
    for _, n_old, n_new in mapped_neighbors:
        distance = abs(n_old - old_rva)
        if distance > 0:
            delta = n_new - n_old
            weight = 1.0 / distance
            weighted_deltas.append((delta, weight))
    
    if weighted_deltas:
        total_weight = sum(w for _, w in weighted_deltas)
        estimated_delta = sum(d * w for d, w in weighted_deltas) / total_weight
        estimated_new_rva = int(old_rva + estimated_delta)
        
        print(f"\nEstimated new RVA for ID {old_id}: 0x{estimated_new_rva:X}")
        
        # Find functions near estimated RVA
        results = []
        for new_id, new_rva in new_db.items():
            if abs(new_rva - estimated_new_rva) <= neighborhood:
                diff = new_rva - estimated_new_rva
                results.append((new_id, new_rva, diff))
        
        results.sort(key=lambda x: abs(x[2]))
        return results
    
    return []


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 1
    
    old_path = sys.argv[1]
    new_path = sys.argv[2]
    
    try:
        old_id = int(sys.argv[3])
    except ValueError:
        print(f"Error: '{sys.argv[3]}' is not a valid ID")
        return 1
    
    search_range = 0x1000
    if '--search-range' in sys.argv:
        idx = sys.argv.index('--search-range')
        if idx + 1 < len(sys.argv):
            try:
                search_range = int(sys.argv[idx + 1], 0)
            except ValueError:
                pass
    
    print(f"Loading old database: {old_path}")
    old_db = decode_address_library(old_path)
    print(f"  {len(old_db):,} entries")
    
    print(f"Loading new database: {new_path}")
    new_db = decode_address_library(new_path)
    print(f"  {len(new_db):,} entries")
    
    # Check if ID exists in old version
    if old_id not in old_db:
        print(f"\nERROR: REL::ID({old_id}) not found in old database!")
        return 1
    
    print(f"\n" + "=" * 70)
    print(f"Searching for equivalent of REL::ID({old_id})")
    print("=" * 70)
    
    # Method 1: RVA Delta analysis
    print("\n[Method 1: Global RVA Delta Analysis]")
    results = find_by_rva_delta(old_db, new_db, old_id, search_range)
    
    if results:
        print(f"\nCandidate functions near predicted RVA:")
        for new_id, new_rva, old_rva, delta_diff in results[:20]:
            print(f"  REL::ID({new_id:>10}): RVA 0x{new_rva:X} (diff: {delta_diff:+#x})")
    
    # Method 2: Neighborhood analysis  
    print("\n" + "-" * 70)
    print("[Method 2: Neighborhood Analysis]")
    neighbors = find_by_neighborhood(old_db, new_db, old_id, search_range)
    
    if neighbors:
        print(f"\nCandidate functions near estimated RVA:")
        for new_id, new_rva, diff in neighbors[:20]:
            print(f"  REL::ID({new_id:>10}): RVA 0x{new_rva:X} (diff: {diff:+#x})")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
