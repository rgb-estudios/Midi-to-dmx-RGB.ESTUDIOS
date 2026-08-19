local function write_result(lines)
  local path = reaper.GetResourcePath() .. "/AEYLA_REAPER_HOST_SMOKE.txt"
  local file = io.open(path, "w")
  if file then
    for _, line in ipairs(lines) do
      file:write(line .. "\n")
    end
    file:close()
  end
  return path
end

local function fail(result, reason)
  table.insert(result, "RESULT=FAIL")
  table.insert(result, "REASON=" .. reason)
  write_result(result)
  reaper.Main_OnCommand(40004, 0)
end

local result = {
  "AEYLA REAPER HOST SMOKE",
  "REAPER_VERSION=" .. reaper.GetAppVersion(),
  "STAGE=STARTUP_SCRIPT",
}
write_result(result)

local found_name = nil
local found_ident = nil
local index = 0
while true do
  local ok, name, ident = reaper.EnumInstalledFX(index)
  if not ok then break end
  local lower_name = string.lower(name or "")
  local lower_ident = string.lower(ident or "")
  if string.find(lower_name, "aeyla visual dmx", 1, true) or
     string.find(lower_ident, "aeylavisualdmx", 1, true) or
     string.find(lower_ident, "aeyla visual dmx", 1, true) then
    found_name = name
    found_ident = ident
    break
  end
  index = index + 1
end

if not found_name then
  fail(result, "AEYLA was not enumerated by REAPER")
  return
end

table.insert(result, "ENUM_NAME=" .. tostring(found_name))
table.insert(result, "ENUM_IDENT=" .. tostring(found_ident))
table.insert(result, "STAGE=ENUMERATED")
write_result(result)

reaper.InsertTrackAtIndex(0, true)
local track = reaper.GetTrack(0, 0)
if not track then
  fail(result, "Could not create test track")
  return
end

local fx_index = -1
if found_ident and found_ident ~= "" then
  fx_index = reaper.TrackFX_AddByName(track, found_ident, false, -1)
end
if fx_index < 0 then
  fx_index = reaper.TrackFX_AddByName(track, found_name, false, -1)
end
if fx_index < 0 then
  fx_index = reaper.TrackFX_AddByName(track, "VST3: AEYLA Visual DMX", false, -1)
end
if fx_index < 0 then
  fx_index = reaper.TrackFX_AddByName(track, "AU: AEYLA Visual DMX", false, -1)
end

if fx_index < 0 then
  fail(result, "REAPER enumerated AEYLA but could not instantiate it")
  return
end

local ok_fx_name, instantiated_name = reaper.TrackFX_GetFXName(track, fx_index)
if not ok_fx_name then instantiated_name = "<unknown>" end
table.insert(result, "INSTANTIATED_NAME=" .. tostring(instantiated_name))
table.insert(result, "FX_COUNT_BEFORE_SAVE=" .. tostring(reaper.TrackFX_GetCount(track)))
table.insert(result, "STAGE=INSTANTIATED")
write_result(result)

-- Exercise the actual plug-in editor, not only the processing component.
-- TrackFX_Show(..., 3) requests a floating editor window. TrackFX_GetOpen
-- must immediately report an open UI; a host/editor crash will also prevent
-- this script from reaching PASS.
reaper.TrackFX_Show(track, fx_index, 3)
reaper.UpdateArrange()
table.insert(result, "STAGE=EDITOR_OPEN_REQUESTED")
write_result(result)
if not reaper.TrackFX_GetOpen(track, fx_index) then
  fail(result, "AEYLA instantiated but its editor did not open")
  return
end
local editor_hwnd = reaper.TrackFX_GetFloatingWindow(track, fx_index)
table.insert(result, "EDITOR_OPEN=1")
table.insert(result, "EDITOR_FLOATING_WINDOW=" .. tostring(editor_hwnd ~= nil and editor_hwnd ~= 0))
table.insert(result, "STAGE=EDITOR_OPEN")
write_result(result)
reaper.TrackFX_Show(track, fx_index, 2)

local project_path = reaper.GetResourcePath() .. "/AEYLA_REAPER_HOST_SMOKE.rpp"
reaper.Main_SaveProjectEx(0, project_path, false)
table.insert(result, "STAGE=PROJECT_SAVED")
write_result(result)

-- Reopen the project from disk so the test exercises the host serialization
-- path, not merely the in-memory instance.
reaper.Main_openProject(project_path)
table.insert(result, "STAGE=PROJECT_REOPENED")
write_result(result)
local reopened_track = reaper.GetTrack(0, 0)
if not reopened_track then
  fail(result, "Saved project reopened without the AEYLA test track")
  return
end

local reopened_count = reaper.TrackFX_GetCount(reopened_track)
table.insert(result, "FX_COUNT_AFTER_REOPEN=" .. tostring(reopened_count))
if reopened_count < 1 then
  fail(result, "AEYLA instance did not survive project save/reopen")
  return
end

local ok_reopened_name, reopened_name = reaper.TrackFX_GetFXName(reopened_track, 0)
if not ok_reopened_name or not string.find(string.lower(reopened_name or ""), "aeyla", 1, true) then
  fail(result, "Reopened FX is not AEYLA")
  return
end

table.insert(result, "REOPENED_NAME=" .. tostring(reopened_name))
write_result(result)

-- Open the restored editor too. This catches projects that deserialize the
-- processing component but leave the graphical/editor side broken.
reaper.TrackFX_Show(reopened_track, 0, 3)
reaper.UpdateArrange()
if not reaper.TrackFX_GetOpen(reopened_track, 0) then
  fail(result, "AEYLA restored after save/reopen but its editor did not reopen")
  return
end
table.insert(result, "EDITOR_REOPEN=1")
table.insert(result, "STAGE=EDITOR_REOPENED")
reaper.TrackFX_Show(reopened_track, 0, 2)

table.insert(result, "RESULT=PASS")
table.insert(result, "SCOPE=HOST_SCAN_INSTANTIATE_EDITOR_SAVE_REOPEN_EDITOR")
write_result(result)
reaper.Main_OnCommand(40004, 0)
