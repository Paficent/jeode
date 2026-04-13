console.setTitle("Jeode Console Test")
for i = 0, 35000 do
    console.writeLine(i);
end

console.hide()
for i = 0, 15000 do
    console.writeLine(i)
end

console.clear()
console.show()

console.write("Display a notification: ")
local str = console.read()
game.displayNotification(str)

console.write("Press enter to hide the console: ")
console.read()
console.hide()
