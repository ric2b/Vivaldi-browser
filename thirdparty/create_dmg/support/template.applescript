on run (volumeName)
  tell application "Finder"
    tell disk (volumeName as string)
      log "Starting to set up volume"
      open

      set theXOrigin to WINX
      set theYOrigin to WINY
      set theWidth to WINW
      set theHeight to WINH

      set theBottomRightX to (theXOrigin + theWidth)
      set theBottomRightY to (theYOrigin + theHeight)
      set dsStore to "\"" & "/Volumes/" & volumeName & "/" & ".DS_STORE\""

      log "Stage 1 done"
      tell container window
        set current view to icon view
        set toolbar visible to false
        set statusbar visible to false
        set the bounds to {theXOrigin, theYOrigin, theBottomRightX, theBottomRightY}
        set statusbar visible to false
        REPOSITION_HIDDEN_FILES_CLAUSE
      end tell

      log "Stage 2 done"
      set opts to the icon view options of container window
      tell opts
        set icon size to ICON_SIZE
        set text size to TEXT_SIZE
        set arrangement to not arranged
      end tell

      log "Stage 3 done"
      BACKGROUND_CLAUSE

      log "Stage 4.1"
      -- Positioning
      POSITION_CLAUSE

      log "Stage 4.2"
      -- Hiding
      HIDING_CLAUSE

      log "Stage 4.3"
      -- Application Link Clause
      APPLICATION_CLAUSE
      close
      open
      log "Stage 4 done"

      -- Force saving of the size
      delay 1

      tell container window
        set statusbar visible to false
        set the bounds to {theXOrigin, theYOrigin, theBottomRightX - 10, theBottomRightY - 10}
      end tell
      log "Stage 5 done"
    end tell

    delay 1

    tell disk (volumeName as string)
      tell container window
        set statusbar visible to false
        set the bounds to {theXOrigin, theYOrigin, theBottomRightX, theBottomRightY}
      end tell
    end tell
    log "Stage 6 done"

    --give the finder some time to write the .DS_Store file
    delay 3

    log "Stage 7 done"
    set waitTime to 0
    set ejectMe to false
    repeat while ejectMe is false
      log "trying to eject container"
      delay 1
      set waitTime to waitTime + 1

      if (do shell script "[ -f " & dsStore & " ]; echo $?") = "0" then set ejectMe to true
    end repeat
    log "waited " & waitTime & " seconds for .DS_STORE to be created."
  end tell
end run
