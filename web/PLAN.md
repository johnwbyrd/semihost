# Zero Board Computer MediaWiki Documentation Plan

## Project Overview

**Goal**: Create comprehensive MediaWiki documentation for the Zero Board Computer (ZBC) specification at www.zeroboardcomputer.com

**Key Points**:
- ZBC is a **platform-agnostic hardware/system specification**, not MAME-specific
- Can be implemented in physical hardware (FPGA/ASIC), emulators, or simulators
- Primary use case: Compiler/debugger/library bring-up with minimal device driver requirements
- Developers get full libc support with only two well-defined devices (MC6847 display + semihosting)

## Implementation Approach

**Content Delivery**: Generate MediaWiki markup files in this repository that can be copied/pasted into the wiki editor

**Workflow**:
1. Create individual `.wiki` files for each page
2. User copies content from files and pastes into MediaWiki editor
3. Allows for review before posting
4. Easy to track changes in version control

## Source Materials

Must read these files to create the documentation:

1. **`/home/jbyrd/git/mame/docs/source/techspecs/zbc.rst`**
   - Comprehensive ZBC system specification (10 sections)
   - Covers: architecture, memory layouts, zbcgen tool, MAME integration

2. **`/home/jbyrd/git/semihost/semihost.md`**
   - RIFF-based semihosting protocol specification (Version 0.1.0)
   - Covers: hardware registers, RIFF protocol, operation modes, syscalls
   - **Important**: Uses `int_size` terminology (not `word_size`)

## Content Organization Strategy

### Reorganization Principles

1. **Platform-Agnostic First**: Emphasize ZBC as a specification, MAME as one implementation
2. **Progressive Detail**: Overview pages → Architecture pages → Reference pages
3. **Self-Contained Pages**: Each page has necessary context to stand alone
4. **Heavy Cross-Linking**: Connect related concepts throughout
5. **No Code Examples Initially**: Focus on accurate core content first

### Page Naming Convention

Use clear, descriptive names without namespace prefixes:
- Good: "System Overview", "Device Registers", "Memory Layout"
- Avoid: "ZBC:Overview", "Semihosting:Registers"

## Complete Page List (48 pages)

### Section 1: Foundation (5 pages)

1. **Main Page** - Landing page with navigation
2. **What is Zero Board Computer?** - Introduction and overview
3. **Design Goals and Use Cases** - Why ZBC exists
4. **Getting Started** - First steps (using MAME as example)
5. **Key Concepts** - Essential terminology

### Section 2: ZBC System Architecture (6 pages)

6. **System Overview** - High-level architecture
7. **Memory Layout and Addressing** - Dynamic calculation, formulas, examples
8. **Video Display (MC6847)** - Text console capabilities
9. **Interrupt System (JP1 Jumper)** - VSync interrupt configuration
10. **CPU Support and Initialization** - Template system, specializations
11. **Quickload System** - Program loading mechanism

### Section 3: Semihosting Protocol (9 pages)

12. **Semihosting Overview** - What it is, why RIFF-based
13. **Device Registers** - Complete 32-byte register map
14. **RIFF Protocol Fundamentals** - Structure, endianness, chunks
15. **CNFG Chunk** - Configuration and architecture declaration
16. **CALL and PARM Chunks** - Request format
17. **DATA Chunk** - Buffers and strings
18. **RETN and ERRO Chunks** - Response format
19. **Operation Modes** - Polling vs interrupts, cache coherency
20. **Syscall Reference** - Complete ARM semihosting compatibility

### Section 4: Implementation Examples (8 pages)

21. **MAME Implementation Overview** - Reference implementation
22. **zbcgen Tool** - Automated CPU discovery
23. **CPU Status Database** - zbc_status.csv schema
24. **Build System Integration** - mame.lst workflow
25. **Adding CPU Support in MAME** - Extending MAME's ZBC
26. **Hardware Implementation Guide** - Building ZBC in FPGA/ASIC
27. **Emulator Implementation Guide** - Adding ZBC to emulators
28. **Implementation Checklist** - Compliance requirements

### Section 5: User Documentation (4 pages)

29. **Running ZBC Systems** - Command-line usage
30. **Writing Programs for ZBC** - Toolchain setup, concepts
31. **Using Semihosting Services** - File I/O, console, timing
32. **Troubleshooting** - FAQ and common issues

### Section 6: Reference Documentation (7 pages)

33. **Complete Register Reference** - All hardware registers
34. **Memory Map Examples** - Layouts for various CPU sizes
35. **Syscall Quick Reference** - Table of operations
36. **RIFF Chunk Reference** - All chunk types
37. **Supported CPU List** - MAME CPU table
38. **Error Codes and Errno Values** - Complete error reference
39. **Glossary** - Technical terms and abbreviations

### Section 7: Developer Documentation (5 pages)

40. **Contributing to ZBC** - Development workflow
41. **Template Specialization Guide** - Adding new CPUs
42. **Protocol Extensions** - Future STRM, EVNT, ABRT, META chunks
43. **Known Limitations** - Excluded CPU types, issues
44. **Future Enhancements** - Roadmap

### Section 8: Infrastructure (4 pages)

45. **Template:Warning** - Warning message template
46. **Template:Note** - Informational note template
47. **Template:SeeAlso** - Cross-reference template
48. **Template:ZBC Navigation** - Main navigation box

## MediaWiki Markup Conventions

```mediawiki
== Level 2 Heading ==
=== Level 3 Heading ===
==== Level 4 Heading ====

* Bullet list
** Nested bullet
# Numbered list
## Nested number

'''Bold text'''
''Italic text''

[[Internal Link]]
[[Page Name|Display Text]]
[https://external.com External Link]

{| class="wikitable"
|-
! Header 1 !! Header 2
|-
| Cell 1 || Cell 2
|}

{{Warning|This is important!}}
{{Note|Informational content}}
{{SeeAlso|Related Page 1|Related Page 2}}

[[Category:Architecture]]
```

## Content Sourcing Matrix

| Wiki Page | Primary Source | Sections |
|-----------|---------------|----------|
| What is Zero Board Computer? | zbc.rst | 1.1, 1.2, 1.3 |
| System Overview | zbc.rst | 2.1, 2.2 |
| Memory Layout | zbc.rst | 2.4 |
| VSync Interrupt | zbc.rst | 2.3 |
| CPU Initialization | zbc.rst | 6 |
| Semihosting Overview | semihost.md | 1 |
| Device Registers | semihost.md | 2 (Hardware Architecture) |
| RIFF Protocol Fundamentals | semihost.md | 3 (RIFF Protocol) |
| CNFG Chunk | semihost.md | 3 (CNFG Chunk section) |
| Operation Modes | semihost.md | 4 |
| Syscall Reference | semihost.md | 8 (ARM Semihosting) |
| zbcgen Tool | zbc.rst | 4 |
| Protocol Extensions | semihost.md | 9 |
| Known Limitations | zbc.rst | 9 |

## Directory Structure

```
~/git/zbc/
├── PLAN.md (this file)
├── README.md (repository overview)
├── pages/
│   ├── 01-foundation/
│   │   ├── Main_Page.wiki
│   │   ├── What_is_Zero_Board_Computer.wiki
│   │   ├── Design_Goals_and_Use_Cases.wiki
│   │   ├── Getting_Started.wiki
│   │   └── Key_Concepts.wiki
│   ├── 02-architecture/
│   │   ├── System_Overview.wiki
│   │   ├── Memory_Layout_and_Addressing.wiki
│   │   ├── Video_Display.wiki
│   │   ├── Interrupt_System.wiki
│   │   ├── CPU_Support_and_Initialization.wiki
│   │   └── Quickload_System.wiki
│   ├── 03-semihosting/
│   │   ├── Semihosting_Overview.wiki
│   │   ├── Device_Registers.wiki
│   │   ├── RIFF_Protocol_Fundamentals.wiki
│   │   ├── CNFG_Chunk.wiki
│   │   ├── CALL_and_PARM_Chunks.wiki
│   │   ├── DATA_Chunk.wiki
│   │   ├── RETN_and_ERRO_Chunks.wiki
│   │   ├── Operation_Modes.wiki
│   │   └── Syscall_Reference.wiki
│   ├── 04-implementation/
│   │   ├── MAME_Implementation_Overview.wiki
│   │   ├── zbcgen_Tool.wiki
│   │   ├── CPU_Status_Database.wiki
│   │   ├── Build_System_Integration.wiki
│   │   ├── Adding_CPU_Support_in_MAME.wiki
│   │   ├── Hardware_Implementation_Guide.wiki
│   │   ├── Emulator_Implementation_Guide.wiki
│   │   └── Implementation_Checklist.wiki
│   ├── 05-user-docs/
│   │   ├── Running_ZBC_Systems.wiki
│   │   ├── Writing_Programs_for_ZBC.wiki
│   │   ├── Using_Semihosting_Services.wiki
│   │   └── Troubleshooting.wiki
│   ├── 06-reference/
│   │   ├── Complete_Register_Reference.wiki
│   │   ├── Memory_Map_Examples.wiki
│   │   ├── Syscall_Quick_Reference.wiki
│   │   ├── RIFF_Chunk_Reference.wiki
│   │   ├── Supported_CPU_List.wiki
│   │   ├── Error_Codes_and_Errno_Values.wiki
│   │   └── Glossary.wiki
│   ├── 07-developer/
│   │   ├── Contributing_to_ZBC.wiki
│   │   ├── Template_Specialization_Guide.wiki
│   │   ├── Protocol_Extensions.wiki
│   │   ├── Known_Limitations.wiki
│   │   └── Future_Enhancements.wiki
│   └── 08-templates/
│       ├── Template_Warning.wiki
│       ├── Template_Note.wiki
│       ├── Template_SeeAlso.wiki
│       └── Template_ZBC_Navigation.wiki
└── reference/
    ├── source-zbc.rst (copy of /home/jbyrd/git/mame/docs/source/techspecs/zbc.rst)
    └── source-semihost.md (copy of /home/jbyrd/git/semihost/semihost.md)
```

## Implementation Phases

### Phase 1: Setup and Templates
- Create directory structure
- Copy source materials to reference/
- Create MediaWiki templates (Warning, Note, SeeAlso, Navigation)
- Create README.md

### Phase 2: Foundation Pages
- Main Page with navigation
- What is Zero Board Computer?
- Design Goals and Use Cases
- Getting Started
- Key Concepts

### Phase 3: Architecture Documentation
- System Overview
- Memory Layout and Addressing
- Video Display (MC6847)
- Interrupt System (JP1)
- CPU Support and Initialization
- Quickload System

### Phase 4: Semihosting Protocol
- Semihosting Overview
- Device Registers
- RIFF Protocol Fundamentals
- CNFG, CALL/PARM, DATA, RETN/ERRO chunks
- Operation Modes
- Syscall Reference

### Phase 5: Implementation Guides
- MAME Implementation (5 pages)
- Building Your Own ZBC (3 pages)

### Phase 6: User Documentation
- Running ZBC Systems
- Writing Programs
- Using Semihosting
- Troubleshooting

### Phase 7: Reference Materials
- Complete Register Reference
- Memory Map Examples
- Syscall Quick Reference
- RIFF Chunk Reference
- CPU List
- Error Codes
- Glossary

### Phase 8: Developer Documentation
- Contributing
- Template Specialization
- Protocol Extensions
- Limitations
- Future Enhancements

### Phase 9: Polish
- Cross-link all pages
- Add category tags
- Verify technical accuracy
- Final review

## Next Steps

1. User reviews this plan
2. Create directory structure and templates
3. Begin Phase 1 implementation
4. Generate wiki pages incrementally
5. User copies pages to MediaWiki as they're created
6. Iterate based on feedback

## Success Criteria

✓ Complete, accurate documentation of ZBC specification
✓ Platform-agnostic presentation (not MAME-centric)
✓ Easy navigation between related topics
✓ Clear reference documentation for all interfaces
✓ MediaWiki markup ready for copy/paste
✓ Consistent formatting throughout
✓ Foundation for adding code examples later
