# RCS: @(#) $Id: span.tcl,v 1.1 2006/07/11 00:11:54 treectrl Exp $

#
# Demo: Column span
#
proc DemoSpan {} {

    set T .f2.f1.t

    #
    # Configure the treectrl widget
    #

    $T configure \
	-showbuttons no \
	-showlines no \
	-showroot no \
	-xscrollincrement 40

    #
    # Create columns
    #

    for {set i 0} {$i < 100} {incr i} {
	$T column create -text "$i" -tag C$i -width 40
    }

    #
    # Create elements
    #

    for {set i 1} {$i <= 20} {incr i} {
	set color gray[expr {50 + $i * 2}]
	$T element create e$i rect -width [expr {$i * 40}] -height 20 \
	    -fill $color -outlinewidth 1 -outline gray70
	$T element create t$i text -text "Span $i"
    }

    #
    # Create styles using the elements
    #

    for {set i 1} {$i <= 20} {incr i} {
	set S [$T style create s$i]
	$T style elements $S [list e$i t$i]
	$T style layout $S e$i -detach yes
	$T style layout $S t$i -expand ns -padx 2
    }

    #
    # Create items and assign styles
    #

    foreach I [$T item create -count 100 -parent root] {
	for {set i 0} {$i < [$T column count]} {} {
	    set span [expr {int(rand() * 20) + 1}]
	    if {$span > [$T column count] - $i} {
		set span [expr {[$T column count] - $i}]
	    }
	    $T item style set $I C$i s$span
	    $T item span $I C$i $span
	    incr i $span
	}
    }

    return
}

