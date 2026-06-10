# Zero Board Computer Wiki Documentation

This directory contains the MediaWiki documentation for the Zero Board Computer
(ZBC), ready to be deployed to www.zeroboardcomputer.com. It lives inside the
ZBC monorepo alongside the protocol specification and the reference
implementations.

> **This wiki is derived documentation.** The single source of truth for the
> semihosting protocol is `docs/source/specification.rst` at the repo root
> (rendered at https://johnwbyrd.github.io/semihost/). When spec content
> changes, edit that file and regenerate the affected wiki pages from it. Do
> not treat these `.wiki` pages as authoritative.

## What is Zero Board Computer?

The Zero Board Computer (ZBC) is a **platform-agnostic hardware/system specification** designed for bringing up compilers, debuggers, and libraries in a highly controlled, deterministic environment. With only two well-defined device drivers (MC6847 text display + semihosting peripheral), developers get full libc support without complex OS infrastructure.

**Key Points:**
- Not MAME-specific - can be built in hardware (FPGA/ASIC), emulators, or simulators
- Minimal hardware: CPU + RAM + Text Display + Semihosting I/O
- Standardized environment for testing and development across any CPU architecture
- RIFF-based semihosting protocol for architecture-agnostic host I/O

## Repository Structure

```
.
├── PLAN.md                    # Complete documentation plan
├── README.md                  # This file
├── pages/                     # All wiki pages organized by section
│   ├── 01-foundation/
│   ├── 02-architecture/
│   ├── 03-semihosting/
│   ├── 04-implementation/
│   ├── 05-user-docs/
│   ├── 06-reference/
│   ├── 07-developer/
│   └── 08-templates/
└── upload_wiki.py             # MediaWiki upload automation
```

(The former `reference/` directory held hand-copied snapshots of the spec.
Those were removed when the website was merged into the monorepo, to eliminate
spec drift. The canonical spec now lives at `docs/source/specification.rst`.)

## Usage

Each `.wiki` file in the `pages/` directory contains MediaWiki markup that can be copied directly into the MediaWiki editor at www.zeroboardcomputer.com.

**To add a page:**
1. Navigate to www.zeroboardcomputer.com
2. Search for the page name (e.g., "System Overview")
3. Click "Create this page"
4. Copy content from the corresponding `.wiki` file
5. Paste into the editor and save

## Source Materials

This documentation is derived from the canonical sources in this monorepo:

1. **Semihosting protocol specification** — `docs/source/specification.rst`
   (the authoritative spec: register map, RIFF protocol, syscall reference).

2. **MAME ZBC system notes** — `web/reference` previously held a copy of the
   MAME `zbc.rst` techspec; refer to the MAME ZBC driver and the spec directly.

## Documentation Structure (48 pages)

### Foundation (5 pages)
- Main Page
- What is Zero Board Computer?
- Design Goals and Use Cases
- Getting Started
- Key Concepts

### ZBC System Architecture (6 pages)
- System Overview
- Memory Layout and Addressing
- Video Display (MC6847)
- Interrupt System (JP1 Jumper)
- CPU Support and Initialization
- Quickload System

### Semihosting Protocol (9 pages)
- Semihosting Overview
- Device Registers
- RIFF Protocol Fundamentals
- CNFG Chunk
- CALL and PARM Chunks
- DATA Chunk
- RETN and ERRO Chunks
- Operation Modes
- Syscall Reference

### Implementation Examples (8 pages)
- MAME Implementation Overview
- zbcgen Tool
- CPU Status Database
- Build System Integration
- Adding CPU Support in MAME
- Hardware Implementation Guide
- Emulator Implementation Guide
- Implementation Checklist

### User Documentation (4 pages)
- Running ZBC Systems
- Writing Programs for ZBC
- Using Semihosting Services
- Troubleshooting

### Reference Documentation (7 pages)
- Complete Register Reference
- Memory Map Examples
- Syscall Quick Reference
- RIFF Chunk Reference
- Supported CPU List
- Error Codes and Errno Values
- Glossary

### Developer Documentation (5 pages)
- Contributing to ZBC
- Template Specialization Guide
- Protocol Extensions
- Known Limitations
- Future Enhancements

### MediaWiki Templates (4 pages)
- Template:Warning
- Template:Note
- Template:SeeAlso
- Template:ZBC Navigation

## Current Status

See PLAN.md for the complete implementation plan and progress.

## Contributing

This repository tracks the authoritative wiki content. The wiki at www.zeroboardcomputer.com becomes the canonical reference, and this repository maintains version-controlled source.

## License

Documentation content: Creative Commons Attribution 4.0 (CC BY 4.0)
