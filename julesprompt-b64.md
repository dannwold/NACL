# JULES - BASE64 ONE-LINE UNPACK INSTRUCTION

The user has uploaded `jules-codebase.b64` to the root of the GitHub repository.
This file is an audited, complete `.tar.gz` archive containing all 103 repository files (`sdk/`, `host_app/`, `docs/`, `.github/`).

Please execute the following command sequence in your terminal:

```bash
# 1. UNPACK CODEBASE DIRECTLY AND OVERWRITE
base64 -d jules-codebase.b64 | tar -xz --overwrite

# 2. STAGE ALL UNPACKED REPOSITORY FILES
git add -A

# 3. SAFELY CLEAN UP CONTAINER ARCHIVE AND PROMPT FILES
git rm -f jules-codebase.b64 julesprompt*.md 2>/dev/null || rm -f jules-codebase.b64 julesprompt*.md

# 4. COMMIT AND PUSH TO MAIN
git commit -m "feat: deploy audited NACL native libraries and Docsify developer site"
git push origin main
```
