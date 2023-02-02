print("Example 1")

-- set up the global functions we want be able to use
Id64ToString=eso.Id64ToString
StringToId64=eso.StringToId64
CompareId64s=eso.CompareId64s
CompareId64ToNumber=eso.CompareId64ToNumber

-- load addon code
eso.LoadAddon("Addon1/Addon1.txt") -- without debug output
local success = eso.LoadAddon("Addon2/Addon2.txt", true) -- with return value and debug output