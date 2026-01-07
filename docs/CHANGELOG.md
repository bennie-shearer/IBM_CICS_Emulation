# IBM CICS (Customer Information Control System) Emulation - Change Log
Version 3.4.6

All notable changes to CICS Emulation are documented here.

## [3.4.6] - 2025-01-07

### Added

- **UUID Module** (libs/uuid)
  - RFC 4122 compliant UUID generation
  - UUID version 4 (random) generation
  - UUID parsing from string
  - String formatting (with/without dashes, upper/lower case)
  - UUIDGenerator class for bulk generation
  - Hash support for use in containers
  - Version and variant detection

- **Compression Module** (libs/compression)
  - Run-Length Encoding (RLE) compression
  - LZ77-style compression with sliding window
  - Auto-detection of best compression method
  - CompressionStats for ratio calculation
  - Compressor class with method selection
  - Huffman frequency table support

### Changed

- Incremented version from 3.4.5 to 3.4.6 across all files
- Total library modules increased from 33 to 35
- Enhanced cross-platform compatibility

### Fixed

- Updated version test in test_types.cpp for patch=6
- All unit tests pass successfully

## [3.4.5] - 2025-01-07

### Added

- **String Utilities Module** (libs/string-utils)
  - Trimming, case conversion, split/join
  - Search/replace, padding, formatting

- **Serialization Module** (libs/serialization)
  - BinaryWriter/BinaryReader classes
  - Byte order support

### Changed

- Updated CMake project name to IBM_CICS_Emulation

## [3.4.4] - 2025-01-07

### Added

- File Utilities Module (libs/file-utils)
- DateTime Module (libs/datetime)
- Configuration Module (libs/config)
- Validation Module (libs/validation)

## [3.4.3] - 2025-01-07

### Fixed

- Fixed version test in test_types.cpp
- Suppressed GCC 13 warnings

## [3.4.2] - 2025-01-01

### Fixed

- Replaced Unicode characters with ASCII-safe alternatives

## [3.4.1] - 2025-12-25

### Fixed

- Version synchronization across all source files

## [3.4.0] - 2025-12-25

### Added

- Document Module (libs/document)
- Inquire Module (libs/inquire)
- Named Counter Module (libs/named-counter)
- Journal Module (libs/journal)

## [3.3.0] - 2025-12-20

### Added

- Syncpoint Module (libs/syncpoint)
- Abend Handler Module (libs/abend-handler)
- Channel Module (libs/channel)
- Terminal Control Module (libs/terminal-control)
- Spool Module (libs/spool)

## [3.2.0] - 2025-12-15

### Added

- Interval Control Module
- Task Control Module
- Storage Control Module
- Program Control Module
- EBCDIC Module
- BMS Module
- Dump Module

## [3.1.0] - 2025-12-10

### Added

- JCL Parser enhancements
- Copybook support

## [3.0.0] - 2025-12-01

### Added

- Initial release
- Core CICS functionality
- VSAM support (KSDS, ESDS, RRDS, LDS)
- GDG support
- TSQ/TDQ support
- Security framework
- Master Catalog emulation
- DFSMShsm integration
