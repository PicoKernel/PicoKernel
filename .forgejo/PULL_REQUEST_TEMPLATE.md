## Summary
<!-- One-line description of what this PR does -->

## Type of Change

- [ ] Bug fix
- [ ] New feature
- [ ] Refactor
- [ ] Documentation / comments only
- [ ] Build / toolchain

## Layer

- [ ] Kernel (`src/kernel/`)
- [ ] Drivers (`src/drivers/`)
- [ ] Modules (`src/modules/`)
- [ ] Interface (`src/interface/`)
- [ ] Build / CI / Docs (no source layer)

## Milestone
<!-- Which milestone does this contribute to? e.g. M5 -->
Target:

## What Changed & Why
<!-- Be specific. "refactored X to fix Y because Z" — not just "fixed stuff" -->

## Testing
<!-- What did you test, how, and what did you observe? -->

- [ ] Compiles cleanly - no warnings: `cmake --build build`
- [ ] Tested on Pico 2W hardware
- [ ] UART output included below (if applicable)
- [ ] Passes Valgrind checks (if applicable)

<details>
<summary>Test output</summary>

```
paste serial / test output here
```

</details>

## Hardware Impact
<!-- Leave blank if not applicable -->

- RAM delta: ~\_\_\_ bytes
- Flash delta: ~\_\_\_ bytes

## Checklist

- [ ] Branch targets `dev`, not `main`
- [ ] No architecture violations - no layer bypasses the kernel, no module or interface calls a driver directly
- [ ] SPDX + file header block on every new or modified `.c` and `.h`
- [ ] All new or modified public functions have Doxygen headers (`@brief`, `@param`, `@return`)
- [ ] No debug output outside `#ifdef KERNEL_DEBUG`
- [ ] PR is focused - one logical change per PR
- [ ] Linked issue: closes #\_\_\_

## WIP?
<!-- Prefix the PR title with WIP: if this is not ready for review -->
