-- The standalone smoke test has no game context. The core must still expose
-- a safe status table and must not fabricate player data.
local status = farever.memory_status()
assert(type(status) == "table")
assert(status.appFound == false)
assert(status.heroFound == false)
assert(status.buildValidated == false)
assert(status.available == false)
assert(farever.player() == nil)
assert(farever.inventory_summary() == nil)
