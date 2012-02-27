Fast serialization for Lua
--------------------------

local marshal = require "marshal"

Provides:
---------

  s = marshal.encode(t)    - serializes a value to a byte stream
  t = marshal.decode(s)    - deserializes a byte stream to a value
  t = marshal.clone(orig)  - deep clone a value (deep for tables and functions)

Features:
---------

Serializes tables, which may contain cycles, Lua functions with upvalues and basic data types.

Limitations:
------------

Coroutines are not serialized and nor are userdata, however support
for userdata the __persist metatable hook can be used.

Metatables and function environments are not serialized.

Cross endianness is not supported.

