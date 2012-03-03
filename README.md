Fast serialization for Lua
==========================

local marshal = require "marshal"

Provides:
---------

* s = marshal.encode(v[, constants])    - serializes a value to a byte stream
* t = marshal.decode(s[, constants])    - deserializes a byte stream to a value
* t = marshal.clone(orig[, constants])  - deep clone a value (deep for tables and functions)

Features:
---------

Serializes tables, which may contain cycles, Lua functions with upvalues and basic data types.

All functions take an optional constants table which, if encountered during serialization,
are simply referenced from the constants table passed during deserialization. For example:

```Lua
local orig = { answer = 42, print = print }
local pack = marshal.encode(orig, { print })
local copy = marshal.decode(pack, { print })
assert(copy.print == print)
```

Hooks
-----

A hook is provided for influencing serializing behaviour via the `__persist` metamethod.
The `__persist` metamethod is expected to return a closure which is called during
deserialization. The return value of the closure is taken as the final decoded result.

This is useful for serializing both userdata and for use with object-oriented Lua,
since metatables are not serialized.

For example:

```Lua
local Point = { }
function Point:new(x, y)
   self.__index = self
   return setmetatable({ x = x, y = y }, self)
end
function Point:move(x, y)
   self.x = x
   self.y = y
end
function Point:__persist()
   local x = self.x
   local y = self.y
   return function()
      -- do NOT refer to self in this scope
      return setmetatable({ x = x, y = y }, Point)
   end
end
```
The above shows a way to persist an "instance" of Point (if you're thinking
in OO terms). In this case `Point` itself will be included in the encoded chunk
because it's referenced as an upvalue of the returned closure.

The `__persist` hook may *NOT* refer to the receiver (i.e. `self`
in the example) because this will cause deep recursion when upvalues
are serialized.

Limitations:
------------

Coroutines are not serialized. Userdata doesn't serialize either
however support for userdata the `__persist` metatable hook can be used.

Metatables and function environments are not serialized.

Attempt to serialize C functions, threads and userdata without a `__persist` hook
raises an exception.

Serialized code is not portable.

