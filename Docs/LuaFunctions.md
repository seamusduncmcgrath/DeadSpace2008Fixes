# Dead Space (2008) Lua function dump

Static dump of every function the game registers into Lua, taken from the GOG `Dead Space.exe` in IDA on 2026-10-31. The game embeds stock Lua 5.0.2 plus tolua. Addresses are for that exe (image base `0x400000`).

## Read this first

- **Two Lua states.** The script manager (`[0xF629D0]`) owns a gameplay state at `+0x14` and an upgrade state at `+0x18`. Every module below is registered in both. `scripts\*.lua` from the script loader run in the gameplay state.
- **Numbers are 32-bit floats** in this build (`lua_Number` is `float`), so integers above 16,777,216 lose precision. `UInt32` / `Int32` exist for exact values.
- **Signatures are inferred**, not declared. Argument count is the highest stack slot the C function reads; types are the type tags or userdata types it checks (`any` means it never checks). Optional arguments show up as extra slots. Returns are the most values it pushes and their types; a count with no type usually means it returns a value it didn't push itself. Treat them as a strong hint and read the C function in IDA when it matters.
- **Methods** are called as `obj:Method(...)`, so slot 1 in their argument list is `obj`.
- **No logging or file output.** `print` and `EclipseLog` go nowhere in retail, and `io`, `os` and `debug` are not loaded.
- **Level scripts are not in here.** They are `LuaScript` resources loaded at runtime that register their functions through `RegisterFunction`, so they only exist in a live game.

## Game functions (globals)

### Movers, player and entity queries

Registered by `sub_4288A0`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `IsMoverValid` | (lightuserdata) | 1: bool | `0x429040` |  |
| `IsRotationValid` | (lightuserdata) | 1: bool | `0x4290A0` |  |
| `GetMoverAttributes` | (lightuserdata or userdata) | 1: table | `0x429100` | table fields: type, position, speed, travelSoundHandle, travelSoundDynamic |
| `GetRotationData` | (lightuserdata) | 1: table | `0x429300` | table fields: curRotation, targetRotation, curSpeed, targetSpeed |
| `GetMoverHandleFromRotateHandle` | (lightuserdata or userdata) | 1 | `0x4294C0` |  |
| `GetInterestedTargetData` | (lightuserdata or userdata) | 1: table | `0x429570` | table fields: myType, myPosition, myTarget, myTargetType, myTargetPosition, distanceFromPlayer |
| `GetPlayerAirValue` | () | 1: number | `0x429860` |  |
| `GetPlayerDistanceTo` | (lightuserdata) | 1: number or nil | `0x429970` |  |
| `IsPlayerInAir` | () | 1: bool | `0x429A70` |  |
| `IsPlayerInGravity` | () | 1: bool | `0x429B30` |  |
| `GetEntityPosition` | (lightuserdata) | 1: lightuserdata | `0x429B80` |  |
| `GetEntityStasis` | (lightuserdata) | 1: number | `0x429BE0` |  |
| `GetEntityDismemberment` | (lightuserdata) | 1: bool or string | `0x429C50` |  |
| `GetCannonRotationSpeed` | (lightuserdata) | 1: number | `0x429DC0` |  |
| `GetPlayerHealth` | () | 1: number | `0x429910` |  |

### Fear

Registered by `sub_4281D0`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `GetFearLevel` | () | 1: number | `0x428280` |  |

### Map

Registered by `sub_42A010`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `GetMapZoom` | () | 1: number | `0x42A0C0` |  |

### Swarm

Registered by `sub_42A2F0`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `GetSwarmCount` | () | 1: number | `0x42A430` |  |

### Global utilities: positions, script registry

Registered by `sub_6F75D0`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `RegisterScriptName` | (number or string, lightuserdata or userdata) | nothing | `0x6F79E0` |  |
| `GetPositionValues` | (userdata) | 3: number | `0x6F7C10` |  |
| `SetPositionValues` | (lightuserdata or userdata, number, number, number) | 1: bool | `0x6F7CE0` |  |
| `GetPlayerPosition` | (number) | 1: lightuserdata or nil | `0x6F7EA0` |  |
| `TEMP_POS` | (number, number, number) | 1: lightuserdata or nil | `0x6F7F80` |  |
| `CreatePosition` | (number, number, number) | 1: userdata | `0x6F80E0` |  |
| `GetPlatform` | () | 1: string | `0x6F8260` | returns "windows" |

### Timers

Registered by `sub_6F8480`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `CreateTimer` | (number or string, bool) | 1: number | `0x6F8970` |  |
| `StartTimer` | (number, number) | nothing | `0x6F8AB0` |  |
| `SetTimerRate` | (number, number) | nothing | `0x6F8C40` |  |
| `StopTimer` | (number) | nothing | `0x6F8E10` |  |
| `DeleteTimer` | (number) | nothing | `0x6F8D30` |  |

### Effects

Registered by `sub_702700`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `GetEffectFunctionValue` | (lightuserdata or userdata, string) | 1: number | `0x702940` |  |
| `GetEffectPosition` | (lightuserdata or userdata) | 1: lightuserdata | `0x702AD0` |  |
| `GetEffectVariable` | (lightuserdata or userdata, number) | 1: number | `0x702B30` |  |
| `SetEffectVariable` | (lightuserdata or userdata, number, number) | nothing | `0x702C50` |  |

### Impacts

Registered by `sub_716310`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `GetImpactData` | (lightuserdata or userdata) | 1: table | `0x7163C0` | table fields: angle, position, materialA, materialB, massA, massB; partly analysed, not a function in the IDB yet |

### Simulation time

Registered by `sub_721220`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `GetSimTime` | () | 1: UInt32 | `0x7212D0` | returns a UInt32 |

### Variables

Registered by `sub_718750`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `FindVariable` | (string) | 1: Variable | `0x718900` |  |
| `VariableGet` | (Variable) | 1: Int32 | `0x718A10` |  |
| `VariableSet` | (Variable, Int32) | 1 | `0x718A70` |  |

### Base objects

Registered by `sub_7128B0`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `Base` | (Base) | 1: Base | `0x7129E0` |  |
| `DeleteBase` | (Base) | 1 | `0x712A00` |  |

### Integer types

Registered by `sub_714F30`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `UInt32` | (UInt32) | 1: UInt32 | `0x7150E0` |  |
| `Int32` | (any) | 1: Int32 | `0x7154C0` |  |
| `Int32ToNumber` | (Int32) | 1: number | `0x7154E0` |  |

### Messaging

Registered by `sub_776140`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `SendMessage` | (string, string, lightuserdata or number or userdata) | 1 | `0x776320` |  |
| `SendMessageEx` | (string) | ? | `0x776510` |  |

### Message receivers

Registered by `sub_772770`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `MessageReceiverNew` | (number) | 1: MessageReceiver | `0x772820` |  |

### Audio: mix

Registered by `sub_6DC450`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `SetMixValues` | (string, number, number, number, number, number) | nothing | `0x6DC690` |  |
| `ApplyMix` | (number or string, bool) | nothing | `0x6DC980` |  |
| `RemoveMix` | (string) | nothing | `0x6DCAF0` |  |
| `SetMasterVolume` | (number) | nothing | `0x6DCC10` |  |

### Audio: ducking

Registered by `sub_707EC0`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `GetDuckCategoryName` | (lightuserdata) | 1: string | `0x707F70` |  |

### Audio: EMX, streams, EARS

Registered by `sub_70E860`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `PlayEmx` | (string, number, number, bool, userdata, number) | 1: lightuserdata | `0x70F5E0` |  |
| `PlayEntityEmx` | (string, number, number, bool, userdata, number) | 1: lightuserdata | `0x70F8D0` |  |
| `StopEmx` | (userdata) | nothing | `0x70FC00` |  |
| `SetEmxVolume` | (lightuserdata or userdata, number) | nothing | `0x70FC80` |  |
| `SetEmxPitch` | (lightuserdata or userdata, number) | nothing | `0x70FD50` |  |
| `SetEmxPosition` | (lightuserdata or userdata, lightuserdata or userdata) | nothing | `0x70FE20` |  |
| `SetEmxAzimuth` | (lightuserdata or userdata, number, number, number) | nothing | `0x70FEB0` |  |
| `PlayStream` | (number, string, number) | nothing | `0x710370` |  |
| `PrimeStream` | (number, string, number) | nothing | `0x710100` |  |
| `StartStream` | (number) | nothing | `0x7102C0` |  |
| `StopStream` | (number, number) | nothing | `0x710530` |  |
| `SetStreamMixCategory` | (number, string) | nothing | `0x710640` |  |
| `SetStreamEmitterLocation` | (number, number) | nothing | `0x710760` |  |
| `SetStreamVolume` | (number, number, number) | nothing | `0x7108A0` |  |
| `SetStreamChannelVolume` | (number, number, number, number) | nothing | `0x710A20` |  |
| `SetStreamEffect` | (number, number, number, number, number, number) | nothing | `0x710C20` |  |
| `SetGlobalEffect` | (number, number, number, number, number, number) | nothing | `0x7110B0` |  |
| `SetPluginEffect` | (string, string, string, number) | nothing | `0x710ED0` |  |
| `SetStreamChannelEffectSend` | (number, number, number) | nothing | `0x711350` |  |
| `GetStreamStatus` | (number) | 1: number | `0x7114C0` |  |
| `GetStreamDuration` | (number) | 1: number | `0x711590` |  |
| `PrimeEarsAudioStream` | (string, number, string) | 1: lightuserdata | `0x711970` |  |
| `PlayEarsAudioStream` | (string or userdata, number, string) | 1: lightuserdata | `0x711770` |  |
| `StopEarsAudioStream` | (userdata) | nothing | `0x711EB0` |  |
| `SetEarsAudioStreamStopMessage` | (lightuserdata, string, ...) | ? | `0x711F80` | partly analysed, not a function in the IDB yet |
| `SetEnvelopeMap` | (number, table, table) | 1: bool or nil | `0x711C90` |  |
| `GetEnvelopeValue` | (number, number) | 1: number | `0x711AF0` |  |

### Script manager

Registered by `sub_719490`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `EclipseLog` | (...) | ? | `0x7192C0` | stub in retail (`mov eax, 1; ret`), does nothing |

### Base library override

Registered by `sub_6DBB30`.

| Function | Arguments | Returns | C function | Notes |
|---|---|---|---|---|
| `print` | (...) | nothing | `0x6DBBB0` | replaces base `print`; output is not shown anywhere in retail |

## Tables

### `GroupManager`

Registered by `sub_7169F0`. Called with `:`, so slot 1 is `GroupManager` itself.

| Function | Arguments | Returns | C function |
|---|---|---|---|
| `GroupManager:RegisterGroup` | (any, string) | 1: Group | `0x716B90` |
| `GroupManager:FindGroup` | (any, string) | 1: Group or nil | `0x716C40` |
| `GroupManager:RemoveFromAllGroups` | (any, Base) | 1 | `0x716CE0` |

## Userdata types

Each type has a metatable made with `luaL_newmetatable`. `__index` is a generic lookup into the metatable (`sub_752750`), so the methods below are what `obj:Method(...)` finds. Metamethods (`__add` and so on) make the operators work.

### `Group`

Returned by `GroupManager:RegisterGroup` / `GroupManager:FindGroup`. Pushed by `sub_716AB0`.

| Method | Arguments | Returns | C function |
|---|---|---|---|
| `AddMember` | (Group, Base) | 1 | `0x716D40` |
| `RemoveMember` | (Group, Base) | 1 | `0x716DD0` |
| `IsMember` | (Group, Base) | 1 | `0x716E60` |
| `Clear` | (Group) | nothing, stub in retail (`mov eax, 1; ret`) | `0x716ED0` |
| `GetMemberCount` | (Group) | 1: UInt32 | `0x716EE0` |
| `ForEachMember` | (Group) | 1 | `0x716F40` |

### `Variable`

Returned by `FindVariable`. Pushed by `sub_718900`.

| Method | Arguments | Returns | C function |
|---|---|---|---|
| `__tostring` | (Variable) | 1: string | `0x718AE0` |
| `Set` | (Variable, Int32) | 1 | `0x718A70` |
| `Get` | (Variable) | 1: Int32 | `0x718A10` |

### `MessageReceiver`

Returned by `MessageReceiverNew`; normally wrapped by `NewMessageReceiver` (see below). Pushed by `sub_772820`.

| Method | Arguments | Returns | C function |
|---|---|---|---|
| `__gc` | (MessageReceiver) | 1 | `0x772930` |
| `AddMessage` | (MessageReceiver, string, string) | 1 | `0x772970` |
| `Remove` | (MessageReceiver, string) | 1 | `0x772A50` |
| `RemoveAllMessages` | (MessageReceiver) | 1 | `0x772AF0` |

### `Base`

Returned by `Base`. Pushed by `sub_712A80`.

| Method | Arguments | Returns | C function |
|---|---|---|---|
| `__gc` | (Base) | 1 | `0x712A00` |
| `__tostring` | (Base) | 1: string | `0x712C30` |

### `UInt32`

Returned by `UInt32` and `GetSimTime`. Pushed by `sub_715100`.

| Method | Arguments | Returns | C function |
|---|---|---|---|
| `__tostring` | (UInt32) | 1: string | `0x7152B0` |
| `__add` | (UInt32, UInt32) | 1: UInt32 | `0x715340` |
| `__sub` | (UInt32, UInt32) | 1: UInt32 | `0x715370` |
| `__mul` | (UInt32, UInt32) | 1: UInt32 | `0x7153A0` |
| `__div` | (UInt32, UInt32) | 1: UInt32 | `0x7153D0` |
| `__eq` | (UInt32, UInt32) | 1: bool | `0x715420` |
| `__lt` | (UInt32, UInt32) | 1: bool | `0x715470` |

### `Int32`

Returned by `Int32`. Pushed by `sub_715510`.

| Method | Arguments | Returns | C function |
|---|---|---|---|
| `__tostring` | (Int32) | 1: string | `0x715690` |
| `__unm` | (Int32) | 1: Int32 | `0x7156F0` |
| `__add` | (Int32, Int32) | 1: Int32 | `0x715750` |
| `__sub` | (Int32, Int32) | 1: Int32 | `0x715780` |
| `__mul` | (Int32, Int32) | 1: Int32 | `0x7157B0` |
| `__div` | (Int32, Int32) | 1: Int32 | `0x7157E0` |
| `__eq` | (Int32, Int32) | 1: bool | `0x715830` |
| `__lt` | (Int32, Int32) | 1: bool | `0x715890` |
| `__le` | (Int32, Int32) | 1: bool | `0x7158F0` |

## Lua-defined globals

Lua source embedded in the exe and run in both states when the modules register.

| Global | Kind | Defined by |
|---|---|---|
| `debug_print(flag, str)` | function, empty | "Global Lua Utility Functions" (`0xD4C810`) |
| `debug_screenprint(flag, str)` | function, empty | "Global Lua Utility Functions" (`0xD4C810`) |
| `gFunctionRegistry` | table: `[scriptName][functionName] = function` | "Global Lua Utility Functions" (`0xD4C468`) |
| `RegisterFunction(scriptName, functionName, functionAddr)` | function | "Global Lua Utility Functions" (`0xD4C468`) |
| `CallLuaFunction(scriptName, functionName, ...)` | function | "Global Lua Utility Functions" (`0xD4C468`) |
| `MessageReceiverRegistry` | table, `{ n = 1 }` plus live receivers by handle | tolua embedded code (`0xE8B0A8`) |
| `MessageReceiverMeta` | table: `__index`, `AddMessage`, `RemoveMessage`, `RemoveAllMessages`, `Delete`. Game bug: its `RemoveMessage` calls `m_pReceiver:RemoveMessage`, but the C method is registered as `Remove`, so it errors | tolua embedded code (`0xE8B0A8`) |
| `MessageReceiverReceive(handle, messageName, dataTypeName, data)` | function | tolua embedded code (`0xE8B0A8`) |
| `NewMessageReceiver(receiveFunction)` | function, returns a receiver object | tolua embedded code (`0xE8B0A8`) |

## Engine-to-Lua entry points

Names the engine looks up in the gameplay state and calls, or sets. Wrapping or replacing these is how a script can react to the game.

| Name | How the engine uses it | Where |
|---|---|---|
| `CallLuaFunction` | called as `CallLuaFunction(scriptName, functionName, ...)` to run a function a level script registered; every `LuaScript` object is sent `"startup"` after it loads | `sub_6F7B60`, `sub_4A72B0` (`call_lua` message), `sub_73EC80` |
| `gGUID` | set to the current `LuaScript` object (lightuserdata) while its chunk and `startup` run, then cleared | `sub_73EC80` |
| `GetSwarmLoopName` | called with no arguments, expected to return a string (swarm audio loop name) | `sub_42A460` |
| `MessageReceiverReceive` | called with `(handle, messageName, dataTypeName, data)` when a receiver gets a message | `sub_7725E0` |

## Standard libraries

- **base (globals):** `error`, `getmetatable`, `setmetatable`, `getfenv`, `setfenv`, `next`, `ipairs`, `pairs`, `print`, `tonumber`, `tostring`, `type`, `assert`, `unpack`, `rawequal`, `rawget`, `rawset`, `pcall`, `xpcall`, `collectgarbage`, `gcinfo`, `loadfile`, `dofile`, `loadstring`, `require`
- **coroutine:** `create`, `wrap`, `resume`, `yield`, `status`
- **table:** `concat`, `foreach`, `foreachi`, `getn`, `setn`, `sort`, `insert`, `remove`
- **string:** `len`, `sub`, `lower`, `upper`, `char`, `rep`, `byte`, `format`, `dump`, `find`, `gfind`, `gsub`
- **math:** `abs`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `ceil`, `floor`, `mod`, `frexp`, `ldexp`, `sqrt`, `min`, `max`, `log`, `log10`, `exp`, `deg`, `pow`, `rad`, `random`, `randomseed`
- `math` also sets the global `__pow` (Lua 5.0 behaviour, powers the `^` operator).
- **Not loaded:** `io`, `os`, `debug`, `loadlib`. Nothing can write files, but `dofile`, `loadfile` and `require` read Lua files from disk through `fopen` (`luaL_loadfile`, `sub_7DBA60`), relative to the game's working directory.
- `tolua` table: `type`, `takeownership`, `releaseownership`, `cast` (opened by `tolua_open`, `sub_7DC3E0`, from the message receiver module and the upgrade bindings).

## Upgrade state only (`+0x18`)

Not reachable from `scripts\*.lua`, which run in the gameplay state.

- **tolua class `UpgradeStruct`:** `At` (`0x5E4280`), `Values` (`0x5E43B0`), `StartPoint` (`0x5E4510`).
- **tolua class `UpgradeManager`:** `GetUpgradeManager` (`0x5E45D0`), `Upgrade` (`0x5E4670`), `Connect` (`0x5E47B0`), `BaseValue` (`0x5E4920`), `UpgradeValue` (`0x5E4AB0`). Registered by `sub_5E4C40`.
- **Precompiled chunk "UpgradeManager"** (`0xE82E28`, 17,692 bytes of Lua 5.0 bytecode, run by `sub_5CDD40`): sets `um = UpgradeManager:GetUpgradeManager()` and defines `InitPlayerUpgrades()`, which builds the upgrade tree for `PlasmaCutter`, `LineGun`, `PulseRifle`, `Ripper`, `Flamethrower`, `ContactBeam`, `ForceGun`, `RIG`, `StasisModule`, `KinesisModule`. Node IDs look like `PC10`, `LG40`, `Rig02`; stats include `Damage`, `Capacity`, `Reload`, `Speed`, `Range`, `HP`, `Air`, `Energy`, `Duration`, `Charge`, `Width`, `AltDamage`.

