# Contributing to PicoKernel

> Full contribution documentation lives in the [Developer Wiki](https://picokernel.codeberg.page/PicoDocs/workflow_contrib.html).
> This file is a quick-start reference.

---

## Before you start

Read these two pages first, they are prerequisites, not suggestions:

- **[Architecture Overview](https://picokernel.codeberg.page/PicoDocs/arch_overview.html)** - the layering rules that govern all code. Violations block merge.
- **[Contributing Workflow](https://picokernel.codeberg.page/PicoDocs/workflow_contrib.html)** - the full PR process.

---

## Quick start

```bash
# Fork on Codeberg, then:
git clone https://codeberg.org/your-username/PicoKernel.git
cd PicoKernel
git submodule update --init --recursive

# Branch from dev - never from main
git checkout dev && git pull origin dev
git checkout -b feat/{subsystem-initial}/your-feature

```

---

## PR checklist

Every PR must satisfy all of the following before it will be reviewed:

- [ ] Branch targets `dev`, not `main`
- [ ] Branch name follows the convention: `feat/<layer>/<name>`, `bugfix/<layer>/<name>` (see [Git Workflow](https://picokernel.codeberg.page/PicoDocs/git_workflow.html))
- [ ] Code compiles cleanly with no warnings: `cmake --build build`
- [ ] No architecture violations, no layer bypasses the kernel, no module calls a driver directly
- [ ] All new or modified public functions have complete Doxygen headers (`@brief`, `@param`, `@return`)
- [ ] SPDX + file header block present on every new or modified `.c` and `.h`
- [ ] No diagnostic output outside `#ifdef KERNEL_DEBUG`
- [ ] PR title, summary, and test evidence are filled. Empty descriptions are returned without review

---

## Key references

| Topic | Wiki page |
|-------|-----------|
| Architecture rules and layer model | [Architecture Overview](https://picokernel.codeberg.page/PicoDocs/arch_overview.html) |
| Branch naming and git strategy | [Git Workflow](https://picokernel.codeberg.page/PicoDocs/git_workflow.html) |
| C naming, formatting, error handling | [Coding Style](https://picokernel.codeberg.page/PicoDocs/coding_style.html) |
| Doxygen templates and enforcement | [Documentation Style](https://picokernel.codeberg.page/PicoDocs/docs_style.html) |
| Memory safety, debug gates, panic policy | [Security Principles](https://picokernel.codeberg.page/PicoDocs/security_principles.html) |
| Licensing, testing, review conduct | [Contributor Expectations](https://picokernel.codeberg.page/PicoDocs/contributor_expectations.html) |
| API reference by layer | [Technical Documentation](https://picokernel.codeberg.page/PicoDocs/technical_docs.html) |

---

## License

PicoKernel source is licensed under **GPL-3.0-or-later**.
Documentation is licensed under **CC BY-SA 4.0**.

By opening a Pull Request, you confirm your contribution is your own work and agree it will be distributed under the license applicable to the file modified.
