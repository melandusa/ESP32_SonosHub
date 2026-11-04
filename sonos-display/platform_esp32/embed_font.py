# PlatformIO pre-build script: embeds LVGL's Montserrat-Medium.ttf as a C array
# (same output as cmake/EmbedFile.cmake in the Linux build).
import os

Import("env")

ttf = os.path.join(env.subst("$PROJECT_LIBDEPS_DIR"), env.subst("$PIOENV"),
                   "lvgl", "scripts", "built_in_font", "Montserrat-Medium.ttf")
out_dir = os.path.join(env.subst("$BUILD_DIR"), "generated_font")
out_file = os.path.join(out_dir, "montserrat_medium_ttf.c")

if not os.path.exists(out_file) or os.path.getmtime(out_file) < os.path.getmtime(ttf):
    os.makedirs(out_dir, exist_ok=True)
    with open(ttf, "rb") as f:
        data = f.read()
    lines = [", ".join("0x%02x" % b for b in data[i:i + 20]) for i in range(0, len(data), 20)]
    with open(out_file, "w") as f:
        f.write("/* Generated from Montserrat-Medium.ttf - do not edit */\n#include <stddef.h>\n")
        f.write("const unsigned char montserrat_medium_ttf[] = {\n%s\n};\n" % ",\n".join(lines))
        f.write("const size_t montserrat_medium_ttf_size = %d;\n" % len(data))

env.BuildSources(os.path.join("$BUILD_DIR", "font"), out_dir)
