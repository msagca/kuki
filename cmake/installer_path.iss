; The Windows installer's offer to put `bin` on the PATH, so `kuki` can be typed in any terminal.
;
; CPack's NSIS generator had this built in (CPACK_NSIS_MODIFY_PATH); its Inno Setup generator has
; nothing like it, so it is written here and included through CPACK_INNOSETUP_EXTRA_SCRIPTS. It is
; a task -- a checkbox on the wizard's own page -- shown only when the CLI is being installed, since
; nothing else in `bin` is meant to be run from a terminal. The engine's and the games' DLLs come
; onto the PATH with it; Windows looks there last for a DLL, after an application's own directory.
;
; The PATH is edited in code rather than by a [Registry] entry, because a [Registry] entry can only
; append `{olddata}` and delete the whole value: on a reinstall it would add the directory a second
; time, and on uninstall it could not take out one entry and leave the rest. Here the directory is
; added only when it is missing, and uninstalling removes that entry alone, whether or not the task
; was ticked, so a PATH edited by hand in between is left as it was otherwise.
;
; The procedures hook the wizard's events by attribute (`<event('...')>`) rather than by the
; event's own name, so they cannot collide with an event function the generated script defines.
[Tasks]
Name: modifypath; Description: "Add the kuki command to the PATH"; Components: KukiCli
[Code]
// An installation for all users edits the machine's PATH, one for the current user only their own.
function KukiEnvironmentRoot: Integer;
begin
  if IsAdminInstallMode then
    Result := HKEY_LOCAL_MACHINE
  else
    Result := HKEY_CURRENT_USER;
end;
function KukiEnvironmentKey: String;
begin
  if IsAdminInstallMode then
    Result := 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment'
  else
    Result := 'Environment';
end;
function KukiBinDirectory: String;
begin
  Result := ExpandConstant('{app}\bin');
end;
// Whether one of the PATH's `;`-separated entries is the directory. Windows compares paths without
// regard to case, so this does too.
function KukiPathHas(Path, Directory: String): Boolean;
begin
  Result := Pos(';' + Uppercase(Directory) + ';', ';' + Uppercase(Path) + ';') > 0;
end;
// The PATH with every entry that is the directory taken out and the others kept as they were, empty
// ones included: only what the installer added is the uninstaller's to remove.
function KukiPathWithout(Path, Directory: String): String;
var
  Rest, Entry: String;
  Separator: Integer;
  First: Boolean;
begin
  Result := '';
  First := True;
  Rest := Path + ';';
  while Rest <> '' do
  begin
    Separator := Pos(';', Rest);
    Entry := Copy(Rest, 1, Separator - 1);
    Delete(Rest, 1, Separator);
    if CompareText(Entry, Directory) <> 0 then
    begin
      if not First then
        Result := Result + ';';
      Result := Result + Entry;
      First := False;
    end;
  end;
end;
// Written back as REG_EXPAND_SZ, the type Windows gives the PATH, so the entries in it that name
// variables (`%SystemRoot%`) keep expanding.
<event('CurStepChanged')>
procedure KukiAddToPath(CurStep: TSetupStep);
var
  Path: String;
begin
  if (CurStep <> ssPostInstall) or not WizardIsTaskSelected('modifypath') then
    Exit;
  if not RegQueryStringValue(KukiEnvironmentRoot, KukiEnvironmentKey, 'Path', Path) then
    Path := '';
  if KukiPathHas(Path, KukiBinDirectory) then
    Exit;
  if (Path <> '') and (Path[Length(Path)] <> ';') then
    Path := Path + ';';
  RegWriteExpandStringValue(KukiEnvironmentRoot, KukiEnvironmentKey, 'Path', Path + KukiBinDirectory);
end;
<event('CurUninstallStepChanged')>
procedure KukiRemoveFromPath(CurUninstallStep: TUninstallStep);
var
  Path: String;
begin
  if CurUninstallStep <> usUninstall then
    Exit;
  if not RegQueryStringValue(KukiEnvironmentRoot, KukiEnvironmentKey, 'Path', Path) then
    Exit;
  if KukiPathHas(Path, KukiBinDirectory) then
    RegWriteExpandStringValue(KukiEnvironmentRoot, KukiEnvironmentKey, 'Path', KukiPathWithout(Path, KukiBinDirectory));
end;
