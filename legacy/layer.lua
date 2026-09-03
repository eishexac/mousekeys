local layer = {}

-- HID usage codes for hidutil remapping
local hidCodes = {
  capslock      = "0x700000039",
  left_control  = "0x7000000E0",
  left_shift    = "0x7000000E1",
  left_option   = "0x7000000E2",
  left_command  = "0x7000000E3",
  right_control = "0x7000000E4",
  right_shift   = "0x7000000E5",
  right_option  = "0x7000000E6",
  right_command = "0x7000000E7",
  section       = "0x700000064",
  f18           = "0x70000006D",
  f19           = "0x70000006E",
  f20           = "0x70000006F",
}

-- macOS virtual keycodes for keys that may not appear in hs.keycodes.map
local virtualKeycodes = {
  capslock      = 57,
  left_control  = 59,  left_shift    = 56,  left_option   = 58,  left_command  = 55,
  right_control = 62,  right_shift   = 60,  right_option  = 61,  right_command = 54,
  f13 = 105, f14 = 107, f15 = 113, f16 = 106,
  f17 = 64,  f18 = 79,  f19 = 80,  f20 = 90,
}

-- Map modifier layer keys to the flag they produce (for stripping from passthrough events)
local keyToModifierFlag = {
  left_control  = "ctrl",  right_control = "ctrl",
  left_shift    = "shift", right_shift   = "shift",
  left_option   = "alt",   right_option  = "alt",
  left_command  = "cmd",   right_command = "cmd",
}

-- Resolve a remap key name or raw HID code to a HID usage code
local function resolveHidCode(key)
  if not key then return nil end
  local code = hidCodes[key]
  if code then return code end
  if key:match("^0x%x+$") then return key end
  return nil
end

local function matchesKey(configKey, keyCode)
  if type(configKey) == "string" then
    return keyCode == configKey
  end
  return keyCode == configKey.primary or keyCode == configKey.secondary
end

local function buildKeyMap(keys)
  local map = {}
  for direction, key in pairs(keys.primary) do
    map[key] = direction
  end
  if keys.secondary then
    for direction, key in pairs(keys.secondary) do
      map[key] = direction
    end
  end
  return map
end

-- hidutil key remapping
local function setHidutilMapping(srcHid, dstHid)
  local cmd = string.format(
    'hidutil property --set \'{"UserKeyMapping":[{"HIDKeyboardModifierMappingSrc":%s,"HIDKeyboardModifierMappingDst":%s}]}\'',
    srcHid, dstHid
  )
  hs.execute(cmd)
end

local function clearHidutilMapping()
  hs.execute('hidutil property --set \'{"UserKeyMapping":[]}\'')
end

-- State
local layerActive = false
local layerToggled = false
local layerKeyDownTime = nil
local layerKeyUsed = false
local layerKeyPhysicalDown = false
local keyTap = nil
local timer = nil
local lastTime = nil

-- Module references (set during init)
local movement = nil
local scroll = nil
local click = nil

-- Config reference (set during init)
local config = nil

-- Shared timer
local function ensureTimerRunning()
  if not timer then
    lastTime = hs.timer.absoluteTime() / 1e9
    timer = hs.timer.doEvery(config.interval, function()
      local now = hs.timer.absoluteTime() / 1e9
      local dt = lastTime and (now - lastTime) or config.interval
      lastTime = now

      movement.update(now, dt)
      scroll.update(now, dt)

      if movement.isIdle() and scroll.isIdle() then
        timer:stop()
        timer = nil
        lastTime = nil
      end
    end)
  end
end

function layer.init(cfg, modules)
  config = cfg
  movement = modules.movement
  scroll = modules.scroll
  click = modules.click
end

function layer.start()
  local movementConfig = {
    initialSpeed = config.initialSpeed,
    acceleration = config.acceleration,
    maxSpeed = config.maxSpeed,
  }

  local scrollConfig = {
    scrollInitialSpeed = config.scrollInitialSpeed,
    scrollAcceleration = config.scrollAcceleration,
    scrollMaxSpeed = config.scrollMaxSpeed,
    scrollFriction = config.scrollFriction,
    scrollTap = config.scrollTap,
    scrollPixelScale = config.scrollPixelScale,
  }

  movement.init(movementConfig, click)
  scroll.init(scrollConfig)

  -- Remap physical key to layer key via hidutil
  local srcHid = resolveHidCode(config.remapKey)
  local dstHid = resolveHidCode(config.layerKey)
  if srcHid and dstHid then
    setHidutilMapping(srcHid, dstHid)
  end

  local keyMap = buildKeyMap(config.keys)
  local layerKeyCode = hs.keycodes.map[config.layerKey] or virtualKeycodes[config.layerKey]
  local layerModFlag = keyToModifierFlag[config.layerKey]

  -- Strip the layer key's modifier flag from an event before passing through
  local function stripLayerModifier(event)
    if layerModFlag then
      local flags = event:getFlags()
      if flags[layerModFlag] then
        flags[layerModFlag] = nil
        event:setFlags(flags)
      end
    end
  end

  keyTap = hs.eventtap.new({ hs.eventtap.event.types.keyDown, hs.eventtap.event.types.keyUp, hs.eventtap.event.types.flagsChanged }, function(event)
    local code = event:getKeyCode()
    local eventType = event:getType()

    -- Layer key handling
    if code == layerKeyCode then
      local isDown
      if eventType == hs.eventtap.event.types.keyDown then
        isDown = true
      elseif eventType == hs.eventtap.event.types.keyUp then
        isDown = false
      elseif eventType == hs.eventtap.event.types.flagsChanged then
        -- Modifier keys don't send keyDown/keyUp; each flagsChanged with
        -- our keycode is a press/release toggle.
        layerKeyPhysicalDown = not layerKeyPhysicalDown
        isDown = layerKeyPhysicalDown
      end

      if isDown then
        if not layerKeyDownTime then
          layerKeyDownTime = hs.timer.absoluteTime() / 1e9
          layerKeyUsed = false
        end
        layerActive = true
      else
        local duration = layerKeyDownTime and (hs.timer.absoluteTime() / 1e9 - layerKeyDownTime) or math.huge
        layerKeyDownTime = nil

        if duration < config.tapTimeout and not layerKeyUsed then
          -- Tap: toggle layer
          layerToggled = not layerToggled
          if layerToggled then
            layerActive = true
          else
            layerActive = false
            movement.releaseAll()
            scroll.releaseAll()
            click.leftUp()
          end
        else
          -- Hold release: only deactivate if not toggled
          if not layerToggled then
            layerActive = false
            movement.releaseAll()
            scroll.releaseAll()
            click.leftUp()
          end
        end
      end
      return true
    end

    if not layerActive then
      stripLayerModifier(event)
      return false
    end

    -- Mark layer key as used (distinguishes hold from tap)
    if eventType == hs.eventtap.event.types.keyDown then
      layerKeyUsed = true
    end

    local keyCode = hs.keycodes.map[code]

    local direction = keyMap[keyCode]
    if direction then
      if eventType == hs.eventtap.event.types.keyDown then
        movement.press(direction)
        ensureTimerRunning()
      else
        movement.release(direction)
      end
      return true
    end

    if matchesKey(config.leftClick, keyCode) then
      if eventType == hs.eventtap.event.types.keyDown then
        click.leftDown()
      else
        click.leftUp()
      end
      return true
    end

    if matchesKey(config.rightClick, keyCode) then
      if eventType == hs.eventtap.event.types.keyDown then
        click.rightClick()
      end
      return true
    end

    if matchesKey(config.scrollUp, keyCode) then
      if eventType == hs.eventtap.event.types.keyDown then
        scroll.press("up")
        ensureTimerRunning()
      else
        scroll.release("up")
      end
      return true
    end

    if matchesKey(config.scrollDown, keyCode) then
      if eventType == hs.eventtap.event.types.keyDown then
        scroll.press("down")
        ensureTimerRunning()
      else
        scroll.release("down")
      end
      return true
    end

    if matchesKey(config.scrollLeft, keyCode) then
      if eventType == hs.eventtap.event.types.keyDown then
        scroll.press("left")
        ensureTimerRunning()
      else
        scroll.release("left")
      end
      return true
    end

    if matchesKey(config.scrollRight, keyCode) then
      if eventType == hs.eventtap.event.types.keyDown then
        scroll.press("right")
        ensureTimerRunning()
      else
        scroll.release("right")
      end
      return true
    end

    -- Let modifier-only events and modifier+key combos pass through (e.g. Cmd+Tab)
    if eventType == hs.eventtap.event.types.flagsChanged then
      stripLayerModifier(event)
      return false
    end
    local flags = event:getFlags()
    local passThroughModifiers = {"cmd", "alt", "ctrl", "fn"}
    for _, mod in ipairs(passThroughModifiers) do
      if flags[mod] then
        stripLayerModifier(event)
        return false
      end
    end
    return true
  end)
  keyTap:start()
end

function layer.stop()
  if keyTap then
    keyTap:stop()
    keyTap = nil
  end
  if timer then
    timer:stop()
    timer = nil
  end
  movement.stop()
  scroll.stop()
  layerActive = false
  layerToggled = false
  layerKeyDownTime = nil
  layerKeyUsed = false
  layerKeyPhysicalDown = false
  lastTime = nil

  -- Restore original key mapping
  local srcHid = resolveHidCode(config.remapKey)
  if srcHid then
    clearHidutilMapping()
  end
end

return layer
