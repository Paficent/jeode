console.setTitle("Jeode Console Test")
for i = 0, 35000 do
    console.writeLine(i);
end

console.hideWindow()
for i = 0, 15000 do
    console.writeLine(i)
end

console.clear()
console.showWindow()

-- Console operations are now synchronous
console.write("Display a notification: ")
local str = console.read()
game.displayNotification(str)

-- Console operations are now synchronous
console.write("Press enter to hide the console: ")
console.read()
console.hideWindow()
