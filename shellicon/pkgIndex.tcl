if {[catch {package require Tcl 8.4}]} return
package ifneeded shellicon 2.0.1 [list load [file join $dir shellicon20.dll] shellicon]
