if {[catch {package require Tcl 8.4}]} return
append script "load \"[file join $dir treectrl20.dll]\" treectrl"
package ifneeded shellicon 2.0.1 [list load [file join $dir shellicon20.dll] shellicon]
