# Copyright (c) 2026 Adam G. Sweeney <AGSweeney@gmail.com>
# SPDX-License-Identifier: MIT
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

if(NOT DEST_DIR OR NOT TRIPLET_BIN)
    message(FATAL_ERROR "CopyVtkRuntime: DEST_DIR and TRIPLET_BIN are required")
endif()
if(NOT EXISTS "${TRIPLET_BIN}")
    message(WARNING "CopyVtkRuntime: missing ${TRIPLET_BIN}")
    return()
endif()
set(_patterns
    vtk*.dll
    vtksys*.dll
    glew32.dll
    freetype.dll
    zlib*.dll
    libpng*.dll
    png*.dll
    jpeg62.dll
    turbojpeg.dll
    lz4.dll
    lzma.dll
    liblzma.dll
    double-conversion.dll
    bz2.dll
    libbz2.dll
    fmt.dll
    pugixml.dll
    libexpat.dll
    brotlicommon.dll
    brotlidec.dll
    tiff.dll
    verdict.dll
    harfbuzz.dll
    md4c.dll
    pcre2-16.dll
    zstd.dll
)
foreach(_pat IN LISTS _patterns)
    file(GLOB _hits "${TRIPLET_BIN}/${_pat}")
    foreach(_dll IN LISTS _hits)
        execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_dll}" "${DEST_DIR}")
    endforeach()
endforeach()
