# Welding Table Generator

Welding Table Generator is a Windows program that builds a parametric welding table and writes the fabrication package. You set the top size, dog holes, apron joints, ribs, openings, tube frame, and sheet nest. The preview updates as you edit. **Generate package** writes full-size DXF files, a sheet nest, and a fabrication PDF.

Design dimensions are inches. Dog-hole diameter can be entered in millimeters or inches. DXF coordinates are inches. Cut only the layers `CUT_OUTER` and `CUT_INNER`.

The user's manual is [docs/Welding_Table_Generator_Users_Manual.md](docs/Welding_Table_Generator_Users_Manual.md). The same manual with the page screenshots is [docs/Welding_Table_Generator_Users_Manual.pdf](docs/Welding_Table_Generator_Users_Manual.pdf).

## What the package contains

Generated files are written to the folder shown on the Output page. A new build puts them in `generator\build\Release\output`.

| File | Contents |
| --- | --- |
| `P01`–`P05` | Top, aprons, and ribs |
| `F01` | Foot plates, cut from the top thickness |
| `N01` | Sheet nest of the plate parts |
| `Q01`, `Q02` | Slot-fit and hole-fit coupons |
| `R01` | Reference frame plan |
| Assembly PDF | Dimensions, parts, nest, and tube sticks |
| `README_Cutting_and_Assembly.md` | Cut notes for that package |

The tube nest is drawn on the Frame view and in the PDF. It is not a separate DXF.

## Build

The app is a Qt 6 Widgets program with a VTK 3D view. DXF files are written with libdxfrw.

Install:

- Windows 10 or later, 64-bit
- Visual Studio 2022, Desktop development with C++
- CMake (the copy installed with Visual Studio is enough)
- [vcpkg](https://github.com/microsoft/vcpkg) Qt 6 and VTK 9.3, installed at `C:\Users\Adam\vcpkg\installed\x64-windows`
- A built libdxfrw at `D:\ANest\build\libdxfrw\Release\dxfrw.lib`

VTK must be the build that uses that same Qt. The build script selects the vcpkg Qt when `share\vtk\vtk-config.cmake` is present there.

From `generator`:

```powershell
.\build.ps1 -Config Release
```

The executable is `generator\build\Release\Welding Table Generator.exe`. The build copies the Qt and VTK DLLs next to it.

Other CMake paths:

- `ANEST_ROOT` defaults to `D:\ANest`. Point it at the tree that contains `third_party\libdxfrw` and the Release `dxfrw.lib`.
- If vcpkg is somewhere else, pass `-DCMAKE_PREFIX_PATH`, `-DQt6_DIR`, and `-DVTK_DIR` when configuring.

Check the Rev A geometry without opening the window:

```powershell
& ".\build\Release\Welding Table Generator.exe" --self-check
```

The report is `generator\build\Release\self-check.txt`. Exit code 0 means the check passed.

Export the Rev A package from the command line:

```powershell
& ".\build\Release\Welding Table Generator.exe" --export ".\build\Release\output"
```

## Installer

[installer/WeldingTableGenerator.iss](installer/WeldingTableGenerator.iss) builds a 64-bit Inno Setup package. It includes the Release program, the Qt and VTK runtime, the Visual C++ runtime, this license, the user's manual, and the libdxfrw sources.

Compile it with Inno Setup 6 after a Release build:

```powershell
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" ".\installer\WeldingTableGenerator.iss"
```

The setup program is `installer\output\WeldingTableGenerator-1.0.0-Setup.exe`. It installs to `C:\Program Files\Welding Table Generator`.

`Libdxfrw` in the script defaults to `D:\ANest\third_party\libdxfrw`. `CrtDir` defaults to the Visual Studio 2022 VC++ redistributable folder used on this machine. Pass `/DLibdxfrw=...` or `/DCrtDir=...` to `ISCC.exe` if those paths differ.

## License

The application source is MIT, copyright Adam G. Sweeney <AGSweeney@gmail.com>. See [LICENSE](LICENSE).

The DXF writer is linked from libdxfrw, which is GPL version 2 or later. Qt is used as separate libraries under the LGPL. VTK is BSD-3-Clause. Details are in [installer/THIRD_PARTY_NOTICES.txt](installer/THIRD_PARTY_NOTICES.txt).
