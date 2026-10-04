# Welding Table Generator

Welding Table Generator is a Windows program that builds a parametric welding table and writes the fabrication package. You set the top size, dog holes, apron joints, ribs, openings, tube frame, and sheet nest. The preview updates as you edit. **Generate package** writes full-size DXF files, a sheet nest, and a fabrication PDF.

Design dimensions are inches. Dog-hole diameter can be entered in millimeters or inches. DXF coordinates are inches. Cut only the layers `CUT_OUTER` and `CUT_INNER`.

The user's manual is [docs/Welding_Table_Generator_Users_Manual.md](docs/Welding_Table_Generator_Users_Manual.md). The same manual with the page screenshots is [docs/Welding_Table_Generator_Users_Manual.pdf](docs/Welding_Table_Generator_Users_Manual.pdf).

## What the package contains

The output path on the form is a job folder. A new install uses `Documents\Welding Table Generator`, because the installed program lives under Program Files and cannot write beside itself. Each successful generate creates a new subfolder such as `WT_24x116_RevA_20261004-090437`. It does not delete older packages or DXF files already in the job folder. If generation fails, that partial folder is removed and the previous package stays.

Part names include the table size and revision. `Package_Manifest.json` records the application version, job id, revision, and a SHA-256 of the settings. The same identity is on the PDF. Overlapping holes, holes through an edge, holes through slots, a contour that crosses itself, a skipped slot or tab, and a nest that was requested but does not fit block the package.

| File | Contents |
| --- | --- |
| `P01`–`P05` | Top, aprons, and ribs |
| `F01` | Foot plates, cut from the top thickness |
| `N01` | Sheet nest of the plate parts |
| `Q01`, `Q02` | Slot-fit and hole-fit coupons |
| `R01` | Reference frame plan |
| Assembly PDF | Dimensions, parts, nest, and tube sticks |
| `README_Cutting_and_Assembly.md` | Cut notes, including measured thickness, coupon approval, and clamp clearance |
| `Package_Manifest.json` | Version, job id, revision, and settings hash |

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

Re-read exported DXFs across a settings sweep. The sweep writes each package into a temporary job folder, opens the DXFs with libdxfrw, and checks inches, AC1018, closed contours, part size, nest borders, and that a failed package does not delete an existing DXF:

```powershell
& ".\build\Release\Welding Table Generator.exe" --sweep
```

The report is `generator\build\Release\sweep.txt`.

Export the Rev A package from the command line:

```powershell
& ".\build\Release\Welding Table Generator.exe" --export ".\build\Release\output"
```

## Installer

[installer/WeldingTableGenerator.iss](installer/WeldingTableGenerator.iss) builds a 64-bit Inno Setup package. It includes the Release program, the Qt and VTK runtime, the Visual C++ runtime, the user's manual, the application source, and the libdxfrw sources.

Compile it with Inno Setup 6 after a Release build:

```powershell
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" ".\installer\WeldingTableGenerator.iss"
```

The setup program is `installer\output\WeldingTableGenerator-1.0.1-Setup.exe`. It installs to `C:\Program Files\Welding Table Generator`. Generated packages go to the user's Documents folder, not under Program Files.

`Libdxfrw` in the script defaults to `D:\ANest\third_party\libdxfrw`. `CrtDir` defaults to the Visual Studio 2022 VC++ redistributable folder used on this machine. Pass `/DLibdxfrw=...` or `/DCrtDir=...` to `ISCC.exe` if those paths differ.

## License

The application source files are MIT, copyright Adam G. Sweeney <AGSweeney@gmail.com>. See [LICENSE](LICENSE).

The executable is statically linked with libdxfrw, which is GPL-2.0-or-later. That combined program is distributed under GPL-2.0-or-later. The installer shows those terms and installs the corresponding source: `source\generator` for this program and `third-party\libdxfrw` for libdxfrw, plus the build steps in this file. Qt is shipped as separate DLLs under the LGPL. VTK is BSD-3-Clause. See [installer/DISTRIBUTION.txt](installer/DISTRIBUTION.txt) and [installer/THIRD_PARTY_NOTICES.txt](installer/THIRD_PARTY_NOTICES.txt).
