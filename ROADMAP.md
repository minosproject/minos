# Minos Roadmap

Minos is an open-source Type-1 hypervisor for embedded and IoT systems. The current roadmap focuses on making the project easier to maintain, easier to port, and more useful for modern embedded platforms.

The next major development phase will focus on two main goals:

1. Refactor the existing codebase to improve maintainability and long-term evolution.
2. Add RISC-V architecture support.

## 1. Codebase Refactoring

Minos was originally developed with ARM-based embedded platforms as the primary target. As the project evolves, the codebase needs clearer architecture boundaries so that new CPU architectures, boards, interrupt controllers, timers, and devices can be supported with less platform-specific coupling.

The refactoring work will focus on:

* Clarifying the separation between architecture-independent hypervisor core logic and architecture-specific code.
* Cleaning up platform and board initialization paths.
* Improving the abstraction of CPU, MMU, interrupt controller, timer, and device virtualization components.
* Reducing duplicated platform code.
* Making the boot and initialization flow easier to understand and debug.
* Improving code readability and documentation for future contributors.
* Keeping behavior compatible with existing supported ARM platforms where possible.

The goal is not to rewrite the project from scratch, but to evolve the current implementation into a cleaner and more portable structure.

## 2. RISC-V Architecture Support

RISC-V is becoming increasingly important in embedded, IoT, automotive, and edge computing systems. Supporting RISC-V is a natural next step for Minos.

The initial RISC-V support will focus on:

* Adding the basic RISC-V architecture layer.
* Supporting machine/supervisor-mode boot flow as required by the target platform.
* Implementing basic CPU context management.
* Adding MMU and page table support for RISC-V.
* Supporting interrupt handling through standard RISC-V interrupt architecture where applicable.
* Adding timer support.
* Bringing up a minimal guest OS on a RISC-V virtual platform.
* Using QEMU RISC-V virt as the first development and validation target.

The first milestone is to boot Minos on a RISC-V virtual platform and run a simple guest workload. After that, the project can gradually expand to more complete guest OS support and real hardware platforms.

## 3. Testing and Validation

To make future development safer, Minos needs better testing and validation workflows.

Planned work includes:

* Adding basic CI checks for build validation.
* Maintaining known-good build targets.
* Adding QEMU-based smoke tests where possible.
* Documenting manual test steps for supported platforms.
* Improving debug logs for boot, CPU initialization, MMU setup, interrupt routing, and guest startup.
* Tracking regressions introduced by architecture refactoring.

The goal is to make it easier to review patches and avoid breaking existing platforms while adding new architecture support.

## 4. Documentation Improvements

Minos should be easier for developers to understand, build, port, and contribute to.

Planned documentation updates include:

* Updating the build instructions.
* Documenting the high-level architecture.
* Describing the boot flow.
* Documenting platform porting steps.
* Adding RISC-V bring-up notes.
* Adding contributor guidelines.
* Adding security reporting guidelines.
* Keeping the roadmap and development status up to date.

## 5. Security Review

As a Type-1 hypervisor, Minos runs at a highly privileged level. Bugs in this layer can affect guest isolation, memory protection, interrupt routing, device access, and overall system integrity.

Security-related work will focus on:

* Reviewing memory isolation logic.
* Reviewing guest-to-hypervisor boundary handling.
* Reviewing interrupt and device access paths.
* Improving validation of guest-provided inputs.
* Reducing unsafe assumptions in low-level code.
* Documenting known security limitations.
* Establishing a clear process for reporting security issues.

This work will be done incrementally together with the refactoring and RISC-V bring-up work.

## 6. Proposed Milestones

### Milestone 1: Maintenance Refresh

* Refresh project documentation.
* Add or update `ROADMAP.md`, `CONTRIBUTING.md`, and `SECURITY.md`.
* Verify existing build instructions.
* Identify currently supported and broken targets.
* Create a list of cleanup tasks for the refactoring phase.

### Milestone 2: Core Refactoring

* Separate common hypervisor logic from architecture-specific code.
* Clean up platform initialization.
* Improve CPU, MMU, interrupt, and timer abstraction layers.
* Keep existing ARM targets buildable where possible.
* Add basic build validation.

### Milestone 3: Initial RISC-V Bring-up

* Add RISC-V architecture skeleton.
* Add QEMU RISC-V virt as the first target.
* Implement basic boot, CPU context, MMU, interrupt, and timer support.
* Boot Minos on QEMU RISC-V virt.
* Start a minimal guest workload.

### Milestone 4: RISC-V Guest Support

* Improve guest boot flow.
* Add basic guest memory and interrupt virtualization.
* Validate simple guest OS scenarios.
* Document RISC-V bring-up and debugging steps.

### Milestone 5: Stabilization

* Improve test coverage.
* Fix regressions found during refactoring.
* Improve documentation for contributors.
* Review security-sensitive code paths.
* Prepare a new tagged release when the codebase becomes stable enough.

## Current Status

Minos is in a maintenance refresh phase. The main development direction is to clean up the existing codebase and prepare the project for RISC-V support.

Contributions, reviews, testing feedback, and platform bring-up reports are welcome.

