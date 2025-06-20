# always keep the .prg, even if subsequent tasks (like running vice) "fails" or in innterrupted
.PRECIOUS: %.prg

FORCE:

%.prg: %.cpp FORCE
	~/c64/oscar64/bin/oscar64 -pp -g -O2 -dNOFLOAT -DNDEBUG -DDBGMSG $<

%.run: %.prg FORCE
	x64sc -autostartprgmode 1 -autostart-warp +cart -moncommands $*.lbl -nativemonitor -device8 1 -iecdevice8 -fs8 . --silent $< 

%.prod: %.cpp FORCE
	~/c64/oscar64/bin/oscar64 -pp -g -dNOLONG -dNOFLOAT -DNDEBUG -O2 -Ox $<

clean:
	@$(RM) *.asm *.int *.lbl *.map *.prg *.bcs *.dbj *.csz
