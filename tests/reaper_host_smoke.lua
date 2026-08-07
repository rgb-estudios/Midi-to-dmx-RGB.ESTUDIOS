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

local result = {
  "AEYLA REAPER HOST SMOKE",
  "REAPER_VERSION=" .. reaper.GetAppVersion(),
}

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
  table.insert(result, "RESULT=FAIL")
  table.insert(result, "REASON=AEYLA was not enumerated by REAPER")
  write_result(result)
  reaper.Main_OnCommand(40004, 0)
  return
end

table.insert(result, "ENUM_NAME=" .. tostring(found_name))
table.insert(result, "ENUM_IDENT=" .. tostring(found_ident))

reaper.InsertTrackAtIndex(0, true)
local track = reaper.GetTrack(0, 0)
if not track then
  table.insert(result, "RESULT=FAIL")
  table.insert(result, "REASON=Could not create test track")
  write_result(result)
  reaper.Main_OnCommand(40004, 0)
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
  table.insert(result, "RESULT=FAIL")
  table.insert(result, "REASON=REAPER enumerated AEYLA but could not instantiate it")
  write_result(result)
  reaper.Main_OnCommand(40004, 0)
  return
end

local ok_fx_name, instantiated_name = reaper.TrackFX_GetFXName(track, fx_index)
if not ok_fx_name then instantiated_name = "<unknown>" end
table.insert(result, "INSTANTIATED_NAME=" .. tostring(instantiated_name))
table.insert(result, "FX_COUNT_BEFORE_SAVE=" .. tostring(reaper.TrackFX_GetCount(track)))

local project_path = reaper.GetResourcePath() .. "/AEYLA_REAPER_HOST_SMOKE.rpp"
reaper.Main_SaveProjectEx(0, project_path, false)

-- Reopen the project from disk so the test exercises the host serialization
-- path, not merely the in-memory instance.
reaper.Main_openProject(project_path)
local reopened_track = reaper.GetTrack(0, 0)
if not reopened_track then
  table.insert(result, "RESULT=FAIL")
  table.insert(result, "REASON=Saved project reopened without the AEYLA test track")
  write_result(result)
  reaper.Main_OnCommand(40004, 0)
  return
end

local reopened_count = reaper.TrackFX_GetCount(reopened_track)
table.insert(result, "FX_COUNT_AFTER_REOPEN=" .. tostring(reopened_count))
if reopened_count < 1 then
  table.insert(result, "RESULT=FAIL")
  table.insert(result, "REASON=AEYLA instance did not survive project save/reopen")
  write_result(result)
  reaper.Main_OnCommand(40004, 0)
  return
end

local ok_reopened_name, reopened_name = reaper.TrackFX_GetFXName(reopened_track, 0)
if not ok_reopened_name or not string.find(string.lower(reopened_name or ""), "aeyla", 1, true) then
  table.insert(result, "RESULT=FAIL")
  table.insert(result, "REASON=Reopened FX is not AEYLA")
  write_result(result)
  reaper.Main_OnCommand(40004, 0)
  return
end

table.insert(result, "REOPENED_NAME=" .. tostring(reopened_name))
table.insert(result, "RESULT=PASS")
table.insert(result, "SCOPE=HOST_SCANNED_AND_STATE_REOPEN")
write_result(result)
reaper.Main_OnCommand(40004, 0)
