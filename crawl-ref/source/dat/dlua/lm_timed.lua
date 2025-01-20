------------------------------------------------------------------------------
-- lm_timed.lua:
-- Lua timed map feature markers.
------------------------------------------------------------------------------

crawl_require('dlua/lm_tmsg.lua')
crawl_require('dlua/lm_1way.lua')

TimedMarker = util.subclass(OneWayStair)
TimedMarker.CLASS = "TimedMarker"

function TimedMarker:new(props)
  props = props or { }

  local tmarker = OneWayStair.new(self, props)

  if not props.msg then
    error("No messaging object provided (msg = nil)")
  end

  props.high = props.high or props.low or props.turns or 1
  props.low  = props.low or props.high or props.turns or 1
  props.high_short = props.high_short or props.low_short or props.turns_short or
                     props.high/10 or 1
  props.low_short = props.low_short or props.high_short or props.turns_short or
                    props.low/10 or 1

  local dur = crawl.random_range(props.low, props.high, props.navg or 1)
  local dur_short = crawl.random_range(props.low_short, props.high_short,
                                       props.navg or 1)
  if fnum == dgn.feature_number('unseen') then
    error("Bad feature name: " .. feat)
  end

  tmarker.started = false
  tmarker.dur = dur * 10
  tmarker.dur_short = dur_short * 10
  if props.single_timed then
    -- Disable LOS timer by setting it as long as the primary timer.
    tmarker.dur_short = tmarker.dur
  end
  tmarker.msg = props.msg
  props.msg = nil

  return tmarker
end

function TimedMarker:activate(marker, verbose)
  self.super.activate(self, marker, verbose)
  self.msg:init(self, marker, verbose)
  dgn.register_listener(dgn.dgn_event_type('entered_level'), marker)
  dgn.register_listener(dgn.dgn_event_type('player_los'), marker, marker:pos())
  dgn.register_listener(dgn.dgn_event_type('turn'), marker)
end

function TimedMarker:property(marker, pname)
  return self.super.property(self, marker, pname)
end

function TimedMarker:start()
  if not self.started then
    self.started = true
  end
end

function TimedMarker:start_short(marker)
  self:start()
  if self.dur_short < self.dur then
     self.dur = self.dur_short
  end
end

function TimedMarker:disappear(marker, x, y)
  dgn.remove_listener(marker)
  self.super.disappear(self, marker, x, y)
end

function TimedMarker:timeout(marker, verbose)
  local x, y = marker:pos()
  local yx, yy = you.pos()

  if x == yx and y == yy and you.taking_stairs() then
    if verbose then
      crawl.mpr( dgn.feature_desc_at(x, y, "The") .. " vanishes " ..
                "just as you enter it!")
      return
    end
  end

  if verbose then
    if you.see_cell(marker:pos()) then
      crawl.mpr( util.expand_entity(self.props.entity, self.props.disappear) or
                 dgn.feature_desc_at(x, y, "The") .. " disappears!")
    else
      crawl.mpr("The walls and floor vibrate strangely for a moment.")
    end
  end

  self:disappear(marker, x, y)
end

function TimedMarker:event(marker, ev)
  if self.super.event(self, marker, ev) then
    return true
  end

  self.ticktype = self.ticktype or dgn.dgn_event_type('turn')

  if ev:type() == dgn.dgn_event_type('entered_level') then
    self:start()
    self.msg:event(self, marker, ev)
  elseif ev:type() == dgn.dgn_event_type('player_los') then
    self:start_short(marker)
  elseif ev:type() == self.ticktype then
    local x, y = marker:pos()
    local yx, yy = you.pos()
    if x == yx and y == yy and you.taking_stairs() and self.dur > 1 then
      self.dur = 1
    else
      self.dur = self.dur - ev:ticks()
    end
    self.msg:event(self, marker, ev)
    if self.dur <= 0 then
      self:timeout(marker, true)
      return true
    end
  end
end

function TimedMarker:describe(marker)
  local feat = self.props.floor or 'floor'
  return feat .. "/" .. tostring(self.dur)
end

function TimedMarker:read(marker, th)
  TimedMarker.super.read(self, marker, th)
  self.started = file.unmarshall_boolean(th)
  self.dur = file.unmarshall_number(th)
  self.dur_short = file.unmarshall_number(th)
  self.msg  = lmark.unmarshall_marker(th)

  setmetatable(self, TimedMarker)
  return self
end

function TimedMarker:write(marker, th)
  TimedMarker.super.write(self, marker, th)
  file.marshall(th, self.started)
  file.marshall(th, self.dur)
  file.marshall(th, self.dur_short)
  lmark.marshall_marker(th, self.msg)
end

function timed_marker(pars)
  return TimedMarker:new(pars)
end
