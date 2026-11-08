# PlatformIO pre-build script: firmware version from the file VERSION (a positive integer).
# The release workflow builds tag vN only if VERSION contains N; the device installs a release
# when its number is higher than its own.
import os

Import("env")

with open(os.path.join(env.subst("$PROJECT_DIR"), "VERSION")) as f:
    version = int(f.read().strip())

env.Append(CPPDEFINES=[("SONOS_FIRMWARE_VERSION", version)])
