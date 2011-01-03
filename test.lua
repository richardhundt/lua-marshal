require "marshal"

local k = { "tkey" }
local a = { "a", "b", "c", [k] = "tval" }
local s = assert(table.marshal(a))
--print(string.format("%q", s))
local t = table.unmarshal(s)
assert(t[1] == "a")
assert(t[2] == "b")
assert(t[3] == "c")
assert(#t == 3)
local _k = next(t, #t)
assert(type(_k) == "table")
assert(_k[1] == "tkey")
assert(t[_k] == "tval")

local up = 69
local s = table.marshal({ answer = 42, funky = function() return up end })
local t = table.unmarshal(s)
assert(t.answer == 42)
assert(type(t.funky) == "function")
assert(t.funky() == up)

local t = { answer = 42 }
local c = { "cycle" }
c.this = c
t.here = c
local s = table.marshal(t)
local u = table.unmarshal(s)
assert(u.answer == 42)
assert(type(u.here) == "table")
assert(u.here == u.here.this)
assert(u.here[1] == "cycle")

local o = { x = 11, y = 22 }
local seen_hook
setmetatable(o, {
   __persist = function(o)
      local x = o.x
      local y = o.y
      seen_hook = true
      local mt = getmetatable(o)
      return function()
         local o = { }
         o.x = x
         o.y = y
         return setmetatable(o, mt)
      end
   end
})

local s = table.marshal({ o })
assert(seen_hook)
local t = table.unmarshal(s)
assert(#t == 1)
local p = t[1]
assert(p ~= o)
assert(p.x == o.x)
assert(p.y == o.y)
assert(getmetatable(p))
assert(type(getmetatable(p).__persist) == "function")

local o = { 42 }
local a = { o, o, o }
local s = table.marshal(a)
local t = table.unmarshal(s)
assert(type(t[1]) == "table")
assert(t[1] == t[2])
assert(t[2] == t[3])

local u = { 42 }
local f = function() return u end
local a = { f, f, u, f }
local s = table.marshal(a)
local t = table.unmarshal(s)
assert(type(t[1]) == "function")
assert(t[1] == t[2])
assert(t[2] == t[4])
assert(type(t[1]()) == "table")
assert(type(t[3]) == "table")
assert(t[1]() == t[3])

local u = function() return 42 end
local f = function() return u end
local a = { f, f, f, u }
local s = table.marshal(a)
local t = table.unmarshal(s)
assert(type(t[1]) == "function")
assert(t[1] == t[2])
assert(t[2] == t[3])
assert(type(t[1]()) == "function")
assert(type(t[4]) == "function")
assert(t[1]() == t[4])

local u = newproxy()
debug.setmetatable(u, {
   __persist = function()
      return function()
         return newproxy()
      end
   end
})

local s = table.marshal{u}
local t = table.unmarshal(s)
assert(type(t[1]) == "userdata")

local t1 = { 1, 'a', b = 'b' }
local t2 = table.clone(t1)

assert(t1[1] == t2[1])
assert(t1[2] == t2[2])
assert(t1.b == t2.b)

print "OK"

--[[ micro-bench (~4.2 seconds on my laptop)
local t = { a='a', b='b', c='c', d='d', hop='jump', skip='foo', answer=42 }
local s = table.marshal(t)
for i=1, 1000000 do
   s = table.marshal(t)
   t = table.unmarshal(s)
end
--]]

