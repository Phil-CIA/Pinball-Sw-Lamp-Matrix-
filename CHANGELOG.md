# Changelog

All notable changes to this project will be documented in this file.

The format follows a simple **Keep a Changelog** style.

## [Unreleased]
### Added
- Repo structure split into `hardware/`, `firmware/`, `docs/`, and `BOM/`
- ESP32-C6 PlatformIO bring-up scaffold
- PCB review checklist and workflow notes
- Repo housekeeping files: `.editorconfig`, `.gitattributes`, `CONTRIBUTING.md`
- GitHub community health files: issue templates, PR template, `SECURITY.md`
- GitHub Actions firmware build workflow and repo setup notes
- `docs/NEXT_ITERATION.md` as a parking-lot file for ideas and items to revisit
- `docs/PINMAP.md` for signal naming, connector grouping, and board-mating notes
- a small SMT test-point recommendation added to BOM and PCB planning notes
- `docs/SCHEMATIC_REVIEW.md` added to track cleanup items before PCB routing
- follow-up schematic review status and block-by-block design notes documented
- updated source review confirmed the comparator/clamp block and larger rail-TVS direction are acceptable
- a practical pre-routing go/no-go checklist was added to the schematic review notes
- current pre-order repo status was documented after production files and routed PCB outputs were added
- `docs/BRINGUP_CHECKLIST.md` added for first-board arrival, assembly, and power-up testing
- restart / wait-for-hardware notes captured from the review chat

### Changed
- Main firmware target aligned to ESP-IDF for the current supported ESP32-C6 board definition
- README, `docs/PROJECT_STATUS.md`, and `hardware/README.md` were updated to reflect the ordered Rev 1 state instead of the earlier placeholder / pre-layout phase
