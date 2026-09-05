import sys
import subprocess

with open(".github/workflows/ndk-build.yml", "r") as f:
    content = f.read()

import yaml
data = yaml.safe_load(content)
print(data)
