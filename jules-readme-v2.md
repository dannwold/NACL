# JULES - NACL DEPLOYMENT & BOOTSTRAP EXECUTIVE DIRECTIVE

This is a direct machine-to-machine instruction set to initialize, unpack, and push the **Android Native Capability Library (NACL)** workspace. 

Follow these steps exactly to complete the bootstrap, trigger the cloud NDK build, and clean up the repository.

---

## Step 1: Download the Master Package

Execute this command in your terminal workspace to retrieve the audited, corrected 81-file bootstrap package:

```bash
curl -sL "https://contribution.usercontent.google.com/download?c=Cgpub3RlYm9va2xtEkASCWFydGlmYWN0cxozCiQyNzdjMjc3Ny1iOTVhLTRhYTktYjk1Yy0yZjk3ZDg0Y2JjN2YSCxIHEPeX7InuDhgB&filename=jules-v3.md&opi=96797242" -o jules-v3.md
```

---

## Step 2: Unpack the Codebase Tree

Create a file named `unpack.py` with the following content and run it. This script uses split-safe string operations (preventing regex and self-parsing escapes) to write all files to their correct directories (`sdk/`, `host_app/`, `.github/workflows/`):

```python
import os
import sys

bootstrap_file = "jules-v3.md"

if not os.path.exists(bootstrap_file):
    print(f"Error: {bootstrap_file} not found!")
    sys.exit(1)

print(f"Parsing and unpacking {bootstrap_file}...")
with open(bootstrap_file, "r", encoding="utf-8") as f:
    content = f.read()

token_begin = "[" + "FILE_PATH_BEGIN:"
token_end = "[" + "FILE_PATH_END" + "]"

blocks = content.split(token_begin)
print(f"Found {len(blocks) - 1} files to extract.")

for block in blocks[1:]:
    if token_end not in block:
        continue
    path, rest = block.split("]", 1)
    path = path.strip()
    
    # Extract file block using corrected list splitting
    code_block = rest.split(token_end)[0].strip()
    
    # Strip markdown fence decorators if present
    lines = code_block.split("\n")
    if lines and lines[0].strip().startswith("```"):
        lines = lines[1:]
    if lines and lines[-1].strip().startswith("```"):
        lines = lines[:-1]
    code = "\n".join(lines).strip()
    
    # Write to correct nested path
    target_path = os.path.join(".", path)
    os.makedirs(os.path.dirname(target_path), exist_ok=True)
    with open(target_path, "w", encoding="utf-8") as out:
        out.write(code)
    print(f"-> Unpacked: {target_path}")

print("\nSUCCESS: All files successfully reconstructed in workspace!")
```

Run the script:
```bash
python3 unpack.py
```

---

## Step 3: Git Commit, Push & Clean Up

Verify that the `sdk/`, `host_app/`, and `.github/` directories exist. Once verified, run these commands to add files to Git tracking, delete the temporary bootstrap files to keep the history clean, and push to trigger the cloud NDK build:

```bash
# Stage the unpacked codebase
git add -A

# Clean up temporary bootstrap and script files from Git and disk
git rm -f jules-v3.md || rm -f jules-v3.md
git rm -f jules-readme.md || rm -f jules-readme.md
git rm -f jules-readme-v2.md || rm -f jules-readme-v2.md
rm -f unpack.py

# Commit and push
git commit -m "feat: complete bootstrap of audited NACL modules and cloud compiler"
git push origin main
```

---

## Step 4: Verify NDK Cloud Build
Monitor the **Actions** tab of your GitHub repository. The `Android NDK Multi-ABI Compiler` workflow will automatically execute. Ensure the dynamic compiler completes successfully with a green checkmark.
