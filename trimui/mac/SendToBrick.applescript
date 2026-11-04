-- Send to Brick: drop games, BIOS files or pictures on this app to copy them to the
-- TrimUI Brick Pro over USB. RaineyUI sorts them into the right folders.
property appTitle : "Send to Brick"

on run
	display dialog "Drag games, BIOS files, pictures or whole folders onto the Send to Brick icon to copy them to your Brick Pro over the USB cable." & return & return & "They're sorted into the right system folders automatically." buttons {"OK"} default button 1 with title appTitle with icon note
end run

on open theItems
	set helper to quoted form of (POSIX path of (path to resource "send.sh"))
	set state to do shell script helper & " check || true"
	if state contains "NOADB" then
		display dialog "adb isn't installed on this Mac, so the Brick can't be reached over USB." buttons {"OK"} default button 1 with title appTitle with icon stop
		return
	else if state is not "OK" then
		display dialog "Brick Pro not found." & return & return & "Plug it in with the USB cable, turn it on, and try again." buttons {"OK"} default button 1 with title appTitle with icon caution
		return
	end if
	set total to count of theItems
	set progress total steps to total
	set progress completed steps to 0
	set progress description to "Copying to Brick Pro…"
	set failed to {}
	with timeout of 86400 seconds
		repeat with i from 1 to total
			set p to POSIX path of (item i of theItems)
			set nm to do shell script "basename " & quoted form of p
			set progress additional description to "(" & i & " of " & total & ") " & nm
			try
				do shell script helper & " push " & quoted form of p
			on error errMsg
				set end of failed to nm
			end try
			set progress completed steps to i
		end repeat
		set progress additional description to "Sorting on the Brick…"
		set report to do shell script helper & " sort"
	end timeout
	set msg to report
	if (count of failed) > 0 then
		set AppleScript's text item delimiters to ", "
		set msg to msg & return & return & "Couldn't copy: " & (failed as text)
		set AppleScript's text item delimiters to ""
	end if
	display dialog "Done!" & return & return & msg buttons {"OK"} default button 1 with title appTitle with icon note
end open
