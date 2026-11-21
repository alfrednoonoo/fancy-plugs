-- Fancy Plugs: colour tracks (docs/reaper-theme.md, sections 4 and 8)
--
-- Opens the suite's 24-colour palette at the mouse: twelve hues in rainbow order, each with a dark
-- shade. Click a colour to give it to the selected tracks, or "Default colour" to clear theirs.
-- Esc or closing the window cancels. Drawn in the suite's menu style (docs/DESIGN_SYSTEM.md): panel
-- #262626, 1 px #434343 border, 26 px rows in 11.5 px #EDEDED, hover #383838 with a 2 px teal bar.

local HUES = {
  { "Red", 0xE74D3D }, { "Orange", 0xE77E23 }, { "Yellow", 0xF1C315 }, { "Lime", 0xA4C639 },
  { "Green", 0x2DCC70 }, { "Teal", 0x00A985 }, { "Cyan", 0x1BB5D4 }, { "Blue", 0x3598DB },
  { "Indigo", 0x5864D6 }, { "Purple", 0x9A59B5 }, { "Magenta", 0xC94FA6 }, { "Pink", 0xE8608A },
}
local DARK = 0.65 -- the dark shades' brightness

local function rgb(hex) return (hex >> 16) & 255, (hex >> 8) & 255, hex & 255 end

-- The 24 swatches: row 1 the colours, row 2 their dark shades.
local swatches = {}
for row = 1, 2 do
  for i, h in ipairs(HUES) do
    local r, g, b = rgb(h[2])
    if row == 2 then
      r, g, b = math.floor(r * DARK + 0.5), math.floor(g * DARK + 0.5), math.floor(b * DARK + 0.5)
    end
    swatches[#swatches + 1] = { name = h[1] .. (row == 2 and " dark" or ""), r = r, g = g, b = b, col = i, row = row }
  end
end

-- Layout in 100 % pixels; everything is multiplied by the display scale.
local PAD, SW, GAP, ROW, HINT = 8, 26, 4, 26, 18
local W = PAD * 2 + 12 * SW + 11 * GAP
local H = PAD + 2 * SW + GAP + PAD + ROW + GAP + HINT + PAD

local function selectedTracks()
  local list = {}
  for i = 0, reaper.CountSelectedTracks(0) - 1 do list[#list + 1] = reaper.GetSelectedTrack(0, i) end
  return list
end

-- The colour every selected track shares, as a native value, or nil.
local function sharedColour(tracks)
  local shared
  for _, tr in ipairs(tracks) do
    local c = math.floor(reaper.GetMediaTrackInfo_Value(tr, "I_CUSTOMCOLOR"))
    if shared == nil then shared = c elseif shared ~= c then return nil end
  end
  return shared
end

local function apply(tracks, sw)
  reaper.Undo_BeginBlock()
  reaper.PreventUIRefresh(1)
  for _, tr in ipairs(tracks) do
    local value = sw and (reaper.ColorToNative(sw.r, sw.g, sw.b) | 0x1000000) or 0
    reaper.SetMediaTrackInfo_Value(tr, "I_CUSTOMCOLOR", value)
  end
  reaper.PreventUIRefresh(-1)
  reaper.TrackList_AdjustWindows(false)
  reaper.UpdateArrange()
  reaper.Undo_EndBlock(sw and ("Fancy: colour tracks " .. sw.name) or "Fancy: default track colour", -1)
end

-- Drawing helpers
local s = 1
local function set(hex, a) local r, g, b = rgb(hex); gfx.set(r / 255, g / 255, b / 255, a or 1) end
-- A filled rounded rectangle, one pixel row at a time on whole pixels: pieces drawn at fractional
-- positions left a light seam in each swatch (user's screenshot, 21 November 2026).
local function fillRound(x, y, w, h, r)
  x, y, w, h = math.floor(x + 0.5), math.floor(y + 0.5), math.floor(w + 0.5), math.floor(h + 0.5)
  r = math.min(math.floor(r + 0.5), math.floor(w / 2), math.floor(h / 2))
  for row = 0, h - 1 do
    local inset = 0
    local d = (row < r) and (r - row - 0.5) or ((row >= h - r) and (row - (h - r) + 0.5) or nil)
    if d then inset = math.floor(r - math.sqrt(math.max(0, r * r - d * d)) + 0.5) end
    gfx.rect(x + inset, y + row, w - 2 * inset, 1, 1)
  end
end
local function check(x, y, size, dark)
  if dark then gfx.set(0.08, 0.08, 0.08, 1) else gfx.set(1, 1, 1, 1) end
  for t = 0, math.max(1, math.floor(1.5 * s)) - 1 do
    gfx.line(x + size * 0.22, y + size * 0.52 + t, x + size * 0.42, y + size * 0.72 + t)
    gfx.line(x + size * 0.42, y + size * 0.72 + t, x + size * 0.78, y + size * 0.32 + t)
  end
end

local function swatchRect(sw)
  local x = PAD + (sw.col - 1) * (SW + GAP)
  local y = PAD + (sw.row - 1) * (SW + GAP)
  return x * s, y * s, SW * s, SW * s
end
local function defaultRect() return PAD * s, (PAD + 2 * SW + GAP + PAD) * s, (W - 2 * PAD) * s, ROW * s end
local function inside(x, y, w, h) return gfx.mouse_x >= x and gfx.mouse_x < x + w and gfx.mouse_y >= y and gfx.mouse_y < y + h end

local wasDown = false

local function frame()
  local ch = gfx.getchar()
  if ch == -1 or ch == 27 then gfx.quit() return end

  local tracks = selectedTracks()
  local shared = sharedColour(tracks)
  local down = gfx.mouse_cap & 1 == 1
  local clicked = wasDown and not down
  wasDown = down

  set(0x262626); gfx.rect(0, 0, gfx.w, gfx.h, 1)
  set(0x434343); gfx.rect(0, 0, gfx.w, gfx.h, 0)

  local hovered
  for _, sw in ipairs(swatches) do
    local x, y, w, h = swatchRect(sw)
    local over = inside(x, y, w, h)
    if over then
      hovered = sw
      gfx.set(1, 1, 1, 1); fillRound(x - 2 * s, y - 2 * s, w + 4 * s, h + 4 * s, 6 * s)
    end
    gfx.set(sw.r / 255, sw.g / 255, sw.b / 255, 1)
    fillRound(x, y, w, h, 4 * s)
    if shared and shared ~= 0 and shared == (reaper.ColorToNative(sw.r, sw.g, sw.b) | 0x1000000) then
      check(x, y, w, 0.299 * sw.r + 0.587 * sw.g + 0.114 * sw.b > 150)
    end
    if over and clicked and #tracks > 0 then apply(tracks, sw); gfx.quit() return end
  end

  local x, y, w, h = defaultRect()
  local overDefault = inside(x, y, w, h)
  if overDefault then
    set(0x383838); fillRound(x, y, w, h, 4 * s)
    set(0x00A985); gfx.rect(x, y + 4 * s, 2 * s, h - 8 * s, 1)
  end
  set(0x8D8D8D); gfx.rect(x + 8 * s, y + 7 * s, 12 * s, 12 * s, 0)
  gfx.line(x + 8 * s, y + 19 * s - 1, x + 20 * s - 1, y + 7 * s)
  set(0xEDEDED); gfx.setfont(1); gfx.x, gfx.y = x + 28 * s, y + (h - gfx.texth) / 2; gfx.drawstr("Default colour")
  if shared == 0 then check(x + w - 24 * s, y + 3 * s, 20 * s, false) end
  if overDefault and clicked and #tracks > 0 then apply(tracks, nil); gfx.quit() return end

  gfx.setfont(2)
  local hint
  if #tracks == 0 then hint = "Select tracks first"
  elseif hovered then hint = hovered.name .. string.format("  #%02X%02X%02X", hovered.r, hovered.g, hovered.b)
  else hint = #tracks == 1 and "1 track selected" or (#tracks .. " tracks selected") end
  set(0x8D8D8D); gfx.x, gfx.y = PAD * s, (H - PAD - HINT) * s + (HINT * s - gfx.texth) / 2; gfx.drawstr(hint)

  gfx.update()
  reaper.defer(frame)
end

-- Open at the mouse, at the display's scale.
gfx.ext_retina = 1
local mx, my = reaper.GetMousePosition()
gfx.init("Fancy: colour tracks", W, H, 0, mx - 20, my - 20)
s = math.max(1, gfx.ext_retina or 1)
if s ~= 1 then gfx.init("", W * s, H * s) end
-- The static "Inter 18pt" family, as the theme uses: the variable font's family "Inter" drew wrong
-- glyphs here ("Defa]lt colo]r"; user's screenshot, 21 November 2026).
-- gfx.setfont's size is the line height, about 1.21 times Inter's font size, so the suite's
-- 11.5 px rows (and an 11 px hint) are multiplied by 1.21 (text too small in the user's screenshot).
gfx.setfont(1, "Inter 18pt", math.floor(11.5 * 1.21 * s + 0.5))
gfx.setfont(2, "Inter 18pt", math.floor(11 * 1.21 * s + 0.5))
frame()
