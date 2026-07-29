# Cobalt Change Log

This project follows [Semantic Versioning](https://semver.org/) (MAJOR.MINOR.PATCH).

## v2.0.0 (2026-07-29) — Architecture Baseline Release

### Changed
- Complete 8-layer architecture implementation (L8 Platform → L1 Applications)
- Added core object system with single inheritance + multi-interface support
- Implemented container hierarchy: Sequence → Vector/List → Map → HashMap/Treemap
- Added algorithm layer with sorting and functional utilities
- Added JSON module and event loop module
- Created comprehensive documentation suite (RFC, SPEC, API, UML, DEV_GUIDE, EXAMPLES, TUTORIAL)

### Added
- `.gitignore` with AI tooling caches exclusion
- CMake build system with pkg-config and find_package support
- `.clangd` configuration for IDE integration
- All header files with include guards
- All source implementations (.c files)
- Specification documents for all 8 layers
- Developer guidelines and contribution workflow

### Fixed
- No bugs in initial baseline release

## v1.0.0 (TBD) — Initial Proposal

### Added
- Initial RFC document proposing the 8-layer architecture concept
- Foundation for object system design discussions

*Note: This is a development branch; public releases will be tagged separately.*
