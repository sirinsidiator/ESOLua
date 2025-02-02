-- loading every single UI file is not possible without implementing all sorts of APIs and likely not even necessary, so we only load some basic library files
-- if you feel something is missing or a file should not be loaded by default, please open an issue on github
local filesToLoad = {
    "/libraries/globals/globalapi.lua",
    "/libraries/globals/localization.lua",
    "/libraries/globals/soundids.lua",
    "/libraries/globals/time.lua",
    "/libraries/globals/debugutils.lua",
    "/libraries/globals/commonconstants.lua",
    "/libraries/utility/baseobject.lua",
    "/libraries/utility/zo_callbackobject.lua",
    "/libraries/utility/zo_colordef.lua",
    "/libraries/utility/zo_tableutils.lua",
    "/libraries/utility/zo_objectpool.lua",
    "/libraries/utility/zo_circularbuffer.lua",
    "/libraries/utility/zo_hook.lua",
    "/libraries/utility/zo_easing.lua",
    "/libraries/utility/zo_platformutils.lua",
    "/libraries/utility/zo_primaryplayername.lua",
    "/libraries/utility/keybindingutils.lua",
    "/libraries/globals/globalvars.lua",
    "/libraries/globals/globals.lua",
    "/libraries/globals/defaultcolordefs.lua",
    "/libraries/zo_gamepad/zo_gamepadlayoutconstants.lua",
    "/libraries/zo_stringsearch/zo_stringsearch.lua",
    "/libraries/utility/zo_autocomplete.lua",
    "/libraries/utility/zo_linkhandler.lua",
    "/libraries/utility/zo_savedvars.lua",
}

-- we need to mock some functions and variables to make the game code work
-- this is just a basic implementation and you may want to replace them in your own code
local function noop() end

eso.verbose = ESOUI_DEBUG or false
local function Log(message)
    if eso.verbose then
        print(message)
    end
end

-- for baseobject.lua
local cvars = {}

function GetCVar()
    return cvars[name] or ""
end

function SetCVar(name, value)
    cvars[name] = "" .. value
end

-- for localization.lua
function GetDigitGroupingSize() return 3 end

local strings = {}
function GetString(prefix, id)
    local key = (prefix or "") .. (id or "")
    return strings[key] or ""
end

function eso.SetString(prefix, id, value)
    local key = (prefix or "") .. (id or "")
    strings[key] = value
end

-- for time.lua
local frameTimeMilliseconds = 0
function GetTimeStamp() return math.floor(GetGameTimeMilliseconds() / 1000) end
function GetFrameTimeMilliseconds() return frameTimeMilliseconds end
function GetFrameTimeSeconds() return frameTimeMilliseconds / 1000 end
function GetGameTimeSeconds() return GetGameTimeMilliseconds() / 1000 end 
function FormatTimeSeconds() return "replace me", 0.5 end

-- for debugutils.lua
DT_LOW = 0
DT_MEDIUM = 1
DT_HIGH = 2
DT_PARENT = 999
DL_BACKGROUND = 0
DL_CONTROLS = 1
DL_TEXT = 2
DL_OVERLAY = 3

-- for commonconstants.lua
function GetMinUICanvasWidth() return 1920 end
function GetMinUICanvasHeight() return 1080 end

-- for zo_platformutils.lua
internalassert = noop

-- for globalvars.lua
function GetWindowManager() return {} end
function GetAnimationManager() return {} end

do
    local EVENT_MANAGER = {}

    local events = {}
    function EVENT_MANAGER:RegisterForEvent(namespace, event, callback)
        if type(namespace) ~= "string" or not event or not callback or (events[event] and events[event][namespace]) then
            return false
        end

        events[event] = events[event] or {}
        events[event][namespace] = callback
        return true
    end

    function EVENT_MANAGER:UnregisterForEvent(namespace, event)
        if type(namespace) == "string" and event and events[event] and events[event][namespace] then
            events[event][namespace] = nil
            return true
        end

        return false
    end

    local pendingEvents = {}
    function eso.TriggerEvent(event, ...)
        if events[event] then
            for _, callback in pairs(events[event]) do
                pendingEvents[#pendingEvents + 1] = { callback = callback, args = {event, ...} }
            end
        end
    end

    local updates = {}
    function EVENT_MANAGER:RegisterForUpdate(namespace, interval, callback)
        Log("Register for update " .. namespace)
        if updates[namespace] then
            Log("Already registered")
            return false
        end
        updates[namespace] = { interval = interval, callback = callback, updateTime = GetGameTimeMilliseconds() + interval }
        return true
    end

    function EVENT_MANAGER:UnregisterForUpdate(namespace)
        Log("Unregister for update " .. namespace)
        if not updates[namespace] then
            Log("Not registered")
            return false
        end
        updates[namespace] = nil
        return true
    end

    local FRAME_RATE_MS = 1000 / 60
    function eso.HandleNextFrame()
        frameTimeMilliseconds = GetGameTimeMilliseconds()

        if #pendingEvents > 0 then
            Log("Run " .. #pendingEvents .. " pending event handlers")
            for _, event in pairs(pendingEvents) do
                local success, err = pcall(event.callback, unpack(event.args))
                if not success then
                    print("Error in event handler: " .. err)
                end
            end
            pendingEvents = {}
        end

        local pendingUpdates = {}
        local closestUpdateTime = math.huge
        for _, entry in pairs(updates) do
            if entry.updateTime <= frameTimeMilliseconds then
                pendingUpdates[#pendingUpdates + 1] = entry
            else
                closestUpdateTime = math.min(closestUpdateTime, entry.updateTime)
            end
        end

        if #pendingUpdates > 0 then
            Log("Run " .. #pendingUpdates .. " pending updates")
            for _, entry in pairs(pendingUpdates) do
                entry.callback()
                entry.updateTime = GetGameTimeMilliseconds() + entry.interval
            end
        end

        local futureUpdateCount = 0
        for _, entry in pairs(updates) do
            futureUpdateCount = futureUpdateCount + 1
        end

        if futureUpdateCount > 0 or #pendingEvents > 0 then
            Log("Waiting for " .. futureUpdateCount .. " updates and " .. #pendingEvents .. " events")
            if futureUpdateCount > 0 then
                local framesToWait = math.ceil((closestUpdateTime - frameTimeMilliseconds) / FRAME_RATE_MS)
                local timeToWait = framesToWait * FRAME_RATE_MS
                Log(string.format("Next update in %.0fms, skipping %d frames", timeToWait, framesToWait - 1))
                eso.Sleep(timeToWait)
            else
                eso.Sleep(FRAME_RATE_MS)
            end
            return eso.HandleNextFrame()
        else
            Log("No updates to wait for")
        end
    end

    function eso.ClearAllEventsAndUpdates()
        Log("Removing all pending events and updates")
        updates = {}
        pendingEvents = {}
    end

    function GetEventManager() return EVENT_MANAGER end
end

-- for defaultcolordefs.lua
function GetInterfaceColor() return 1, 1, 1, 1 end
COMBAT_MECHANIC_FLAGS_MAGICKA = 1
COMBAT_MECHANIC_FLAGS_WEREWOLF = 2
COMBAT_MECHANIC_FLAGS_STAMINA = 4
COMBAT_MECHANIC_FLAGS_MOUNT_STAMINA = 16
COMBAT_MECHANIC_FLAGS_HEALTH = 32
CHAMPION_DISCIPLINE_TYPE_COMBAT = 0
CHAMPION_DISCIPLINE_TYPE_CONDITIONING = 1
CHAMPION_DISCIPLINE_TYPE_WORLD = 2
ALLIANCE_ALDMERI_DOMINION = 1
ALLIANCE_EBONHEART_PACT = 2
ALLIANCE_DAGGERFALL_COVENANT = 3
BATTLEGROUND_TEAM_FIRE_DRAKES = 1
BATTLEGROUND_TEAM_PIT_DAEMONS = 2
BATTLEGROUND_TEAM_STORM_LORDS = 3
STATUS_EFFECT_TYPE_POISON = 4
STATUS_EFFECT_TYPE_DISEASE = 8
STATUS_EFFECT_TYPE_WOUND = 11
STATUS_EFFECT_TYPE_MAGIC = 21
TEX_SAMPLE_PROCESSING_RGB = 0
TEX_SAMPLE_PROCESSING_ALPHA_AS_RGB = 1

local function tryLoadIngameSource()
    for _, file in ipairs(filesToLoad) do
        if not eso.LoadLuaFile(ESOUI_SOURCE_PATH .. file, true) then
            return false
        end
    end
    return true
end

if tryLoadIngameSource() then
    SecurePostHook = ZO_PostHook
    print("ESO Lua initialized successfully")
else
    print("Failed to initialize ESO Lua from folder '" .. ESOUI_SOURCE_PATH .. "'")
    print("Make sure you have placed the ESOUI source files from https://github.com/esoui/esoui/ in ./esoui in your working directory, provide a different path via -s or use -d to provide more information about what went wrong")
end

ESOUI_SOURCE_PATH = nil
ESOUI_DEBUG = nil