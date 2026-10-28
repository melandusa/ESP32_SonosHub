# Converts a binary file into a C array.
# Usage: cmake -DINPUT=<file> -DOUTPUT=<file.c> -DSYMBOL=<name> -P EmbedFile.cmake
file(READ "${INPUT}" hex HEX)
file(SIZE "${INPUT}" size)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
string(REGEX REPLACE "((0x..,){20})" "\\1\n" bytes "${bytes}")
file(WRITE "${OUTPUT}"
  "/* Generated from ${INPUT} - do not edit */\n"
  "#include <stddef.h>\n"
  "const unsigned char ${SYMBOL}[] = {\n${bytes}\n};\n"
  "const size_t ${SYMBOL}_size = ${size};\n")
