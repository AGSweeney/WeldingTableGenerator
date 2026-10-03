; Copyright (c) 2026 Adam G. Sweeney <AGSweeney@gmail.com>
; SPDX-License-Identifier: MIT
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in all
; copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
; SOFTWARE.
;
; Compile from this folder:
;   "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" WeldingTableGenerator.iss

#ifndef CrtDir
#define CrtDir "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.44.35112\x64\Microsoft.VC143.CRT"
#endif
#ifndef Libdxfrw
#define Libdxfrw "D:\ANest\third_party\libdxfrw"
#endif

#define MyAppName "Welding Table Generator"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Adam G. Sweeney"
#define MyAppExeName "Welding Table Generator.exe"
#define BuildDir "..\generator\build\Release"

[Setup]
AppId={{8F3C1A6E-5B24-4D7A-9C18-2E6F4A9B7D01}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL=mailto:AGSweeney@gmail.com
AppSupportURL=mailto:AGSweeney@gmail.com
AppCopyright=Copyright (c) 2026 Adam G. Sweeney
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoCopyright=Copyright (c) 2026 Adam G. Sweeney
VersionInfoDescription={#MyAppName} Setup
VersionInfoProductName={#MyAppName}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputDir=output
OutputBaseFilename=WeldingTableGenerator-{#MyAppVersion}-Setup
SetupIconFile=..\generator\resources\icons\app\welding-table.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#BuildDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\generic\*"; DestDir: "{app}\generic"; Flags: ignoreversion
Source: "{#BuildDir}\iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion
Source: "{#BuildDir}\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion
Source: "{#BuildDir}\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion
Source: "{#BuildDir}\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion
Source: "{#BuildDir}\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion
Source: "{#BuildDir}\tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion
Source: "{#CrtDir}\msvcp140.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#CrtDir}\msvcp140_1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#CrtDir}\msvcp140_2.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#CrtDir}\vcruntime140.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#CrtDir}\vcruntime140_1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "THIRD_PARTY_NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\docs\Welding_Table_Generator_Users_Manual.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "..\docs\manual\*"; DestDir: "{app}\docs\manual"; Excludes: "capture.ps1"; Flags: ignoreversion
Source: "{#Libdxfrw}\COPYING"; DestDir: "{app}\third-party\libdxfrw"; Flags: ignoreversion
Source: "{#Libdxfrw}\AUTHORS"; DestDir: "{app}\third-party\libdxfrw"; Flags: ignoreversion
Source: "{#Libdxfrw}\README.md"; DestDir: "{app}\third-party\libdxfrw"; Flags: ignoreversion
Source: "{#Libdxfrw}\CMakeLists.txt"; DestDir: "{app}\third-party\libdxfrw"; Flags: ignoreversion
Source: "{#Libdxfrw}\src\*"; DestDir: "{app}\third-party\libdxfrw\src"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\User's Manual"; Filename: "{app}\docs\Welding_Table_Generator_Users_Manual.md"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
