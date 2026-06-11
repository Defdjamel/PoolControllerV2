"""
sync_version.py — PlatformIO pre-build script
Lit FIRMWARE_VERSION dans src/config.h et met a jour firmware/version.json.
Source de verite : config.h. version.json est genere, ne pas editer manuellement.
"""
Import("env")
import re, json, os

project_dir = env.subst("$PROJECT_DIR")

# Lire la version depuis config.h
config_path = os.path.join(project_dir, "src", "config.h")
with open(config_path) as f:
    content = f.read()

m = re.search(r'#define\s+FIRMWARE_VERSION\s+"([^"]+)"', content)
if not m:
    print("\033[33m[sync_version] FIRMWARE_VERSION introuvable dans config.h\033[0m")
    exit(0)

version = m.group(1)

# Mettre a jour firmware/version.json
json_path = os.path.join(project_dir, "firmware", "version.json")
with open(json_path) as f:
    data = json.load(f)

old = data.get("version", "")
if old == version:
    print(f"[sync_version] version.json a jour ({version})")
else:
    data["version"] = version
    data["url"] = (
        f"https://github.com/Defdjamel/PoolControllerV2/releases/download/"
        f"v{version}/firmware.bin"
    )
    with open(json_path, "w") as f:
        json.dump(data, f, indent=2)
        f.write("\n")
    print(f"\033[32m[sync_version] version.json mis a jour : {old} -> {version}\033[0m")
