#!/usr/bin/env python3
"""
Generate fuzzer seed corpus with valid RIFF structure but malicious sizes.

Creates test cases with:
- Negative sizes (0xFFFFFFFF = -1)
- Zero sizes
- Huge sizes (bigger than buffer)
- Size that would overflow when added to offset
- Sizes that point just past valid data
"""

import os
import struct
import sys

# Output directory: command-line arg or default to script location
if len(sys.argv) > 1:
    CORPUS_DIR = sys.argv[1]
else:
    CORPUS_DIR = os.path.join(os.path.dirname(__file__), "corpus", "riff_parser")

os.makedirs(CORPUS_DIR, exist_ok=True)

def write_corpus(name, data):
    """Write a corpus file."""
    path = os.path.join(CORPUS_DIR, name)
    with open(path, "wb") as f:
        f.write(data)
    print(f"  {name}: {len(data)} bytes")

def u32le(val):
    """Pack as little-endian uint32."""
    return struct.pack("<I", val & 0xFFFFFFFF)

def fourcc(s):
    """Create FourCC from string."""
    return s.encode("ascii")

# RIFF header: "RIFF" + size + form_type
# Chunk header: fourcc + size
# CNFG payload: int_size(1) + ptr_size(1) + endianness(1) + reserved(1) = 4 bytes

print("Generating malformed corpus files...")

# --- RIFF size edge cases ---

# RIFF with size = 0 (too small for form type)
write_corpus("malformed_riff_size_zero",
    fourcc("RIFF") + u32le(0) + fourcc("SEMI"))

# RIFF with size = -1 (0xFFFFFFFF)
write_corpus("malformed_riff_size_neg1",
    fourcc("RIFF") + u32le(0xFFFFFFFF) + fourcc("SEMI"))

# RIFF with size = 4 (just form type, no chunks) - valid edge case
write_corpus("malformed_riff_size_min",
    fourcc("RIFF") + u32le(4) + fourcc("SEMI"))

# RIFF with size bigger than buffer (claims 1000 bytes but only 12 present)
write_corpus("malformed_riff_size_huge",
    fourcc("RIFF") + u32le(1000) + fourcc("SEMI"))

# RIFF with size that would overflow: 0xFFFFFFF0 + 8 = wrap around
write_corpus("malformed_riff_size_overflow",
    fourcc("RIFF") + u32le(0xFFFFFFF0) + fourcc("SEMI"))


# --- Chunk size edge cases ---

# CNFG chunk with size = 0 (no payload)
write_corpus("malformed_cnfg_size_zero",
    fourcc("RIFF") + u32le(12) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(0))

# CNFG chunk with size = -1
write_corpus("malformed_cnfg_size_neg1",
    fourcc("RIFF") + u32le(12) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(0xFFFFFFFF))

# CNFG chunk with size bigger than remaining buffer
write_corpus("malformed_cnfg_size_huge",
    fourcc("RIFF") + u32le(12) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(1000) + bytes([4, 4, 0, 0]))

# CNFG chunk with size = 3 (odd, needs padding check)
write_corpus("malformed_cnfg_size_odd",
    fourcc("RIFF") + u32le(12) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(3) + bytes([4, 4, 0]))


# --- CALL chunk with sub-chunk size issues ---

# CALL with size = 0 (no opcode header)
write_corpus("malformed_call_size_zero",
    fourcc("RIFF") + u32le(20) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(4) + bytes([4, 4, 0, 0]) +
    fourcc("CALL") + u32le(0))

# CALL with size = -1
write_corpus("malformed_call_size_neg1",
    fourcc("RIFF") + u32le(20) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(4) + bytes([4, 4, 0, 0]) +
    fourcc("CALL") + u32le(0xFFFFFFFF) + bytes([0x01, 0, 0, 0]))

# CALL with PARM sub-chunk that has size = -1
write_corpus("malformed_parm_size_neg1",
    fourcc("RIFF") + u32le(36) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(4) + bytes([4, 4, 0, 0]) +
    fourcc("CALL") + u32le(16) + bytes([0x01, 0, 0, 0]) +  # opcode header
    fourcc("PARM") + u32le(0xFFFFFFFF) + bytes([0, 0, 0, 0, 0x42, 0, 0, 0]))

# CALL with PARM that claims more than remaining CALL data
write_corpus("malformed_parm_size_overflow",
    fourcc("RIFF") + u32le(36) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(4) + bytes([4, 4, 0, 0]) +
    fourcc("CALL") + u32le(16) + bytes([0x01, 0, 0, 0]) +
    fourcc("PARM") + u32le(1000) + bytes([0, 0, 0, 0, 0x42, 0, 0, 0]))


# --- DATA chunk size issues ---

# DATA with size = -1
write_corpus("malformed_data_size_neg1",
    fourcc("RIFF") + u32le(36) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(4) + bytes([4, 4, 0, 0]) +
    fourcc("CALL") + u32le(16) + bytes([0x01, 0, 0, 0]) +
    fourcc("DATA") + u32le(0xFFFFFFFF) + bytes([0, 0, 0, 0]) + b"hello")

# DATA with size = 0 (empty data)
write_corpus("malformed_data_size_zero",
    fourcc("RIFF") + u32le(28) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(4) + bytes([4, 4, 0, 0]) +
    fourcc("CALL") + u32le(12) + bytes([0x01, 0, 0, 0]) +
    fourcc("DATA") + u32le(0))


# --- RETN chunk size issues ---

# RETN with size = 0
write_corpus("malformed_retn_size_zero",
    fourcc("RIFF") + u32le(12) + fourcc("SEMI") +
    fourcc("RETN") + u32le(0))

# RETN with size = -1
write_corpus("malformed_retn_size_neg1",
    fourcc("RIFF") + u32le(12) + fourcc("SEMI") +
    fourcc("RETN") + u32le(0xFFFFFFFF) + bytes([0, 0, 0, 0, 0, 0, 0, 0]))

# RETN with size = 4 (missing errno)
write_corpus("malformed_retn_size_short",
    fourcc("RIFF") + u32le(16) + fourcc("SEMI") +
    fourcc("RETN") + u32le(4) + bytes([0, 0, 0, 0]))


# --- ERRO chunk size issues ---

# ERRO with size = 0
write_corpus("malformed_erro_size_zero",
    fourcc("RIFF") + u32le(12) + fourcc("SEMI") +
    fourcc("ERRO") + u32le(0))

# ERRO with size = 1 (too small for error code)
write_corpus("malformed_erro_size_one",
    fourcc("RIFF") + u32le(14) + fourcc("SEMI") +
    fourcc("ERRO") + u32le(1) + bytes([0x01, 0]))

# ERRO with size = -1
write_corpus("malformed_erro_size_neg1",
    fourcc("RIFF") + u32le(12) + fourcc("SEMI") +
    fourcc("ERRO") + u32le(0xFFFFFFFF))


# --- Nested container issues ---

# Multiple chunks where second chunk's offset + size wraps
write_corpus("malformed_multi_chunk_wrap",
    fourcc("RIFF") + u32le(24) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(4) + bytes([4, 4, 0, 0]) +
    fourcc("CALL") + u32le(0x7FFFFFFF))  # huge size

# Chunk that ends exactly at buffer end
write_corpus("malformed_chunk_exact_end",
    fourcc("RIFF") + u32le(12) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(4) + bytes([4, 4, 0, 0]))

# Chunk header present but no room for payload
write_corpus("malformed_chunk_header_only",
    fourcc("RIFF") + u32le(8) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(4))  # claims 4 bytes but nothing follows


# --- Integer boundary cases for int_size parsing ---

# CNFG with int_size = 0
write_corpus("malformed_cnfg_int_size_zero",
    fourcc("RIFF") + u32le(12) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(4) + bytes([0, 4, 0, 0]))  # int_size=0

# CNFG with int_size = 255
write_corpus("malformed_cnfg_int_size_huge",
    fourcc("RIFF") + u32le(12) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(4) + bytes([255, 4, 0, 0]))  # int_size=255

# CNFG with ptr_size = 0
write_corpus("malformed_cnfg_ptr_size_zero",
    fourcc("RIFF") + u32le(12) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(4) + bytes([4, 0, 0, 0]))  # ptr_size=0


# --- Malformed CNFG + CALL combinations (triggers response writing) ---

# CNFG with huge int_size + valid CALL (triggers write_retn overflow)
write_corpus("malformed_cnfg_huge_with_call",
    fourcc("RIFF") + u32le(28) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(4) + bytes([255, 4, 0, 0]) +  # int_size=255
    fourcc("CALL") + u32le(4) + bytes([0x13, 0, 0, 0]))  # SYS_ERRNO opcode

# CNFG with int_size=0 + valid CALL
write_corpus("malformed_cnfg_zero_with_call",
    fourcc("RIFF") + u32le(28) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(4) + bytes([0, 4, 0, 0]) +  # int_size=0
    fourcc("CALL") + u32le(4) + bytes([0x13, 0, 0, 0]))  # SYS_ERRNO opcode

# CNFG with ptr_size=0 + valid CALL
write_corpus("malformed_cnfg_ptr_zero_with_call",
    fourcc("RIFF") + u32le(28) + fourcc("SEMI") +
    fourcc("CNFG") + u32le(4) + bytes([4, 0, 0, 0]) +  # ptr_size=0
    fourcc("CALL") + u32le(4) + bytes([0x13, 0, 0, 0]))  # SYS_ERRNO opcode


print(f"\nGenerated files in {CORPUS_DIR}")
