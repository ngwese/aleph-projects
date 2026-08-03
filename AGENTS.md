# Agent instructions

## Build commands

When running build commands, prefix them with `aleph-builder` if it is available.
For example:

```sh
aleph-builder make
```

`aleph-builder` provides all necessary toolchains to build AVR32 and Blackfin
targets.

Apps and modules in this repo build against `vendor/aleph`. Ensure submodules
are initialized (`git submodule update --init --recursive`) before building.

## Commit messages

Use [Conventional Commits](https://www.conventionalcommits.org/) style for commit
messages. Wrap subject and body lines to 80 characters.

Format:

```text
<type>(<optional scope>): <short summary>

<optional body wrapped to 80 characters>
```

Examples:

```text
fix(between): handle missing preset file on boot

Return a clear error when the default preset path does not exist instead of
continuing with uninitialized state.
```

```text
refactor(mx44): extract crossover coefficient helper
```

## Markdown style

Prefer lowercase for markdown you generate: titles, headers, sentences, and
normal prose. Exceptions: common emphasis tokens like `FIXME` / `TODO`, and
acronyms (e.g. DSP, SD, AVR32).
