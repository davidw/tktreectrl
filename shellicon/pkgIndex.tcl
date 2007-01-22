if {[catch {package require Tcl 8.4}]} return
package ifneeded shellicon 2.2.2 [list load [file join $dir shellicon22.dll] shellicon]
