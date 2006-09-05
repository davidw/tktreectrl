if {[catch {package require Tcl 8.4}]} return
package ifneeded shellicon 2.1.1 [list load [file join $dir shellicon21.dll] shellicon]
