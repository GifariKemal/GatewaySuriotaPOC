# Code Formatting Guide

**Version:** 1.0.0 | **Last Updated:** January 2, 2026

This document describes how to format code in the GatewaySuriotaPOC project to maintain
consistent code style across all files.

---

## Overview

| File Type        | Formatter     | Style    | Config               |
| ---------------- | ------------- | -------- | -------------------- |
| C++ (.cpp, .h)   | clang-format  | Google   | Built-in             |
| Python (.py)     | black         | PEP 8    | Default              |
| Markdown (.md)   | prettier      | Default  | --prose-wrap always  |

---

## Prerequisites

### 1. Install LLVM (for clang-format)

```bash
# Windows (via Chocolatey)
choco install llvm -y

# Verify installation
clang-format --version
```

### 2. Install Black (Python formatter)

```bash
# Install via pip
python -m pip install black

# Verify installation
python -m black --version
```

### 3. Install Prettier (Markdown formatter)

```bash
# Requires Node.js
npm install -g prettier

# Or use npx (no install needed)
npx prettier --version
```

---

## Manual Formatting Commands

### Format C++ Files (Google Style)

```bash
# Format single file
clang-format -i --style=Google Main/ModbusRtuService.cpp

# Format all C++ files in Main folder
clang-format -i --style=Google Main/*.cpp Main/*.h

# Preview changes without modifying (dry run)
clang-format --style=Google Main/ModbusRtuService.cpp
```

**Google Style Highlights:**

- 2-space indentation
- 80-character line limit
- Spaces after `if`, `for`, `while`
- Opening brace on same line

### Format Python Files (PEP 8 with Black)

```bash
# Format single file
python -m black Tools/sign_firmware.py

# Format all Python files in project
python -m black .

# Preview changes without modifying (check mode)
python -m black --check .

# Show diff of changes
python -m black --diff Tools/sign_firmware.py
```

**Black Style Highlights:**

- 4-space indentation
- 88-character line limit (configurable)
- Double quotes for strings
- Trailing commas in multi-line structures

### Format Markdown Files

```bash
# Format single file
npx prettier --write Documentation/README.md

# Format all Markdown files
npx prettier --write "**/*.md"

# Format with prose wrapping (recommended)
npx prettier --write --prose-wrap always "**/*.md"

# Preview changes without modifying
npx prettier --check "**/*.md"
```

**Prettier Markdown Highlights:**

- Consistent list indentation
- Normalized spacing
- Table alignment
- Code block formatting

---

## Format All Files (One Command)

Create a batch script or run these commands sequentially:

### Windows (PowerShell)

```powershell
# Format all code files
clang-format -i --style=Google Main/*.cpp Main/*.h
python -m black .
npx prettier --write --prose-wrap always "**/*.md"
```

### Windows (Batch)

Create `format_all.bat`:

```batch
@echo off
echo Formatting C++ files...
"C:\Program Files\LLVM\bin\clang-format.exe" -i --style=Google Main\*.cpp Main\*.h

echo Formatting Python files...
python -m black .

echo Formatting Markdown files...
npx prettier --write --prose-wrap always "**/*.md"

echo Done!
```

---

## VS Code Integration

### Recommended Extensions

1. **C/C++** (Microsoft) - Includes clang-format support
2. **Black Formatter** (Microsoft) - Python formatting
3. **Prettier - Code formatter** (Prettier) - Markdown and more

### Settings (settings.json)

```json
{
  // C++ formatting
  "C_Cpp.clang_format_style": "Google",
  "C_Cpp.clang_format_fallbackStyle": "Google",
  "[cpp]": {
    "editor.defaultFormatter": "ms-vscode.cpptools",
    "editor.formatOnSave": true
  },

  // Python formatting
  "[python]": {
    "editor.defaultFormatter": "ms-python.black-formatter",
    "editor.formatOnSave": true
  },

  // Markdown formatting
  "[markdown]": {
    "editor.defaultFormatter": "esbenp.prettier-vscode",
    "editor.formatOnSave": true
  }
}
```

### Keyboard Shortcut

- **Format Document:** `Ctrl + Shift + I` (or `Shift + Alt + F`)
- **Format Selection:** `Ctrl + K, Ctrl + F`

---

## Pre-commit Hook (Optional)

To automatically format code before each commit, create `.git/hooks/pre-commit`:

```bash
#!/bin/sh
# Format staged C++ files
for file in $(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(cpp|h)$'); do
    clang-format -i --style=Google "$file"
    git add "$file"
done

# Format staged Python files
for file in $(git diff --cached --name-only --diff-filter=ACM | grep -E '\.py$'); do
    python -m black "$file"
    git add "$file"
done
```

Make it executable:

```bash
chmod +x .git/hooks/pre-commit
```

---

## Troubleshooting

### clang-format not found

```bash
# Check if LLVM is in PATH
where clang-format

# If not found, add to PATH or use full path
"C:\Program Files\LLVM\bin\clang-format.exe" --version
```

### Black not formatting

```bash
# Ensure black is installed for the correct Python
python -m pip show black

# Reinstall if needed
python -m pip install --upgrade black
```

### Prettier not working

```bash
# Check Node.js installation
node --version

# Use npx to run without global install
npx prettier --version
```

---

## Style Reference

### C++ (Google Style)

- Official guide: https://google.github.io/styleguide/cppguide.html
- clang-format docs: https://clang.llvm.org/docs/ClangFormat.html

### Python (Black/PEP 8)

- Black documentation: https://black.readthedocs.io/
- PEP 8 guide: https://peps.python.org/pep-0008/

### Markdown (Prettier)

- Prettier docs: https://prettier.io/docs/en/index.html

---

**Made with care by SURIOTA R&D Team**
