---
name: shell-platform
description: >-
  Always detect the OS shell before running commands. This project is on Windows
  (PowerShell). Never use bash/linux commands directly — use PowerShell equivalents.
  Use when executing ANY terminal command (build, test, git, file ops, etc.).
---

# Shell platform awareness

**This project runs on Windows.** The user's shell is **PowerShell** (`pwsh.exe`).

## Rules

1. **Before running ANY shell command**, confirm it is compatible with PowerShell.
2. **Never** use bash-only utilities (`head`, `tail`, `grep`, `cat`, `ls`, `rm`, `cp`, `mv`, `which`, `export`, `source`, `make`, `sed`, `awk`) without converting to PowerShell equivalents.
3. **Never** use `&&` to chain commands — PowerShell uses `;` or pipeline.
4. **Never** use `echo $VAR` — PowerShell uses `Write-Host $VAR` or just `$VAR`.
5. **Never** use `#!/bin/bash` or assume a Unix shell is available.
6. If a tool or script is bash-only, wrap it: `bash -c "..."` only if Git Bash / WSL is confirmed available, otherwise rewrite in PowerShell.

## Common conversions

| Bash / Linux | PowerShell (Windows) |
|--------------|----------------------|
| `ls` | `Get-ChildItem` or `dir` |
| `cat file` | `Get-Content file` |
| `head -n 10` | `Get-Content file \| Select-Object -First 10` |
| `tail -n 10` | `Get-Content file \| Select-Object -Last 10` |
| `grep "pattern" file` | `Select-String -Path file -Pattern "pattern"` |
| `rm file` | `Remove-Item file` |
| `cp src dst` | `Copy-Item src dst` |
| `mv src dst` | `Move-Item src dst` |
| `mkdir -p dir` | `New-Item -ItemType Directory -Force -Path dir` |
| `export VAR=val` | `$env:VAR = "val"` |
| `which cmd` | `Get-Command cmd` |
| `echo "text" > file` | `"text" \| Out-File file` |
| `chmod +x file` | Not needed (Windows uses file associations) |
| `pwd` | `Get-Location` or `(Get-Location).Path` |
| `cmd1 && cmd2` | `cmd1; cmd2` or `cmd1; if ($?) { cmd2 }` |
| `$(command)` | `$(command)` (same syntax, but subexpression rules differ) |
| `make build` | Use the actual build command (e.g., `idf.py build`, `cmake --build .`) |
| `find . -name "*.cpp"` | `Get-ChildItem -Recurse -Filter "*.cpp"` |
| `wc -l file` | `(Get-Content file).Count` |
| `sed 's/a/b/' file` | `(Get-Content file) -replace 'a','b'` |
| `xargs` | `ForEach-Object { ... }` |

## ESP-IDF specific

ESP-IDF commands work the same on Windows (they are Python scripts):

```powershell
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py flash monitor
```

CMake also works cross-platform:

```powershell
cmake -S host_test -B host_test/build
cmake --build host_test/build
ctest --test-dir host_test/build --output-on-failure
```

## Git

Git commands are the same on Windows (Git for Windows provides `git.exe`):

```powershell
git status
git add .
git commit -m "message"
git log --oneline -10
```

## When bash IS needed

If a script or tool explicitly requires bash (e.g., some CI scripts, `configure` scripts):

1. Check if Git Bash is available: `Get-Command bash -ErrorAction SilentlyContinue`
2. Run via: `& "C:\Users\darvik\scoop\apps\git\current\bin\bash.exe" -c "command"`
3. Or use WSL if available: `wsl command`
4. **Ask the user** if neither is confirmed.

## Quick check template

Before running any command, ask yourself:
- [ ] Is this PowerShell-compatible?
- [ ] Am I using `;` instead of `&&`?
- [ ] Am I using `Get-Content` instead of `cat`?
- [ ] Am I using `Select-String` instead of `grep`?
- [ ] Am I using `$env:VAR` instead of `export VAR`?
