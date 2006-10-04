# RCS: @(#) $Id: rowlabels.tcl,v 1.1 2006/10/04 04:11:27 treectrl Exp $

proc DemoRowLabels {} {

    global RowLabel

    set T .f2.f1.t

    #
    # Configure the treectrl widget
    #

    $T configure \
	-showbuttons no \
	-showlines no \
	-showroot no \
	-xscrollincrement 40 \
	-showrowlabels yes

    #
    # Create columns
    #

    for {set i 0} {$i < 100} {incr i} {
	$T column create -text "C$i" -tags C$i -width 40 -justify center
    }

    $T state define mouseover

    #
    # Create row labels
    #

    $T element create label1.bg rect -fill {gray80 mouseover gray {}}
    $T element create label1.text text
    $T style create label1 -orient horizontal
    $T style elements label1 {label1.bg label1.text}
    $T style layout label1 label1.bg -detach yes -iexpand xy
    $T style layout label1 label1.text -expand wns -padx 2
    
    for {set i 0} {$i < 10} {incr i} {
	$T rowlabel create -tags R$i -style label1
	$T rowlabel text R$i "R$i"
    }

    $T element create label2.bd border -background $::SystemButtonFace \
	-relief raised -thickness 2 -filled yes
    $T element create label2.text text
    $T style create label2 -orient horizontal
    $T style elements label2 {label2.bd label2.text}
    $T style layout label2 label2.bd -detach yes -iexpand xy
    $T style layout label2 label2.text -expand news -padx 2 -pady 2
    
    for {set i 10} {$i < 20} {incr i} {
	$T rowlabel create -tag R$i -style label2
	$T rowlabel text R$i "R$i"
    }

    $T element create label3.div rect -fill black -height 2
    $T element create label3.text text
    $T style create label3 -orient horizontal
    $T style elements label3 {label3.div label3.text}
    $T style layout label3 label3.div -detach yes -expand n -iexpand x
    $T style layout label3 label3.text -expand ws -padx 2 -pady 2
    
    for {set i 20} {$i < 30} {incr i} {
	$T rowlabel create -tag R$i -style label3
	$T rowlabel text R$i "R$i"
    }

    $T element create label4.rect rect -fill {#e0e8f0 mouseover}
    $T element create label4.text text
    $T element create label4.w window -clip yes
    $T style create label4 -orient vertical
    $T style elements label4 {label4.rect label4.text label4.w}
    $T style layout label4 label4.rect -detach yes -iexpand xy
    $T style layout label4 label4.text -expand we -padx 2 -pady 2
    $T style layout label4 label4.w -iexpand x -padx 2 -pady {0 2}
    
    for {set i 30} {$i < 40} {incr i} {
	$T rowlabel create -tag R$i -style label4
	$T rowlabel element configure R$i label4.text -textvariable ::DemoRowLabels(R$i)
	set clip [frame $T.clip$i -borderwidth 0]
	$::entryCmd $clip.e -width 4 -textvariable ::DemoRowLabels(R$i)
	$T rowlabel element configure R$i label4.w -window $clip
	set ::DemoRowLabels(R$i) "R$i"
    }

    #
    # Create elements
    #

    $T element create eRect rect -outline gray -outlinewidth 1

    #
    # Create styles using the elements
    #

    $T style create s1 -orient horizontal
    $T style elements s1 eRect
    $T style layout s1 eRect -detach yes -iexpand xy

    #
    # Create items and assign styles
    #

    set ids [$T item create -count 40 -parent root]
    foreach C [$T column list] {
	$T item style set [list list $ids] $C s1
    }

    bind DemoRowLabels <Motion> {
	RowLabelMotion %W %x %y
    }

    set RowLabel(prev) ""
    bindtags $T [list $T DemoRowLabels TreeCtrl [winfo toplevel $T] all]

    return
}

proc RowLabelMotion {w x y} {
    global RowLabel
    set id [$w identify $x $y]
    if {$id eq ""} {
    } elseif {[lindex $id 0] eq "header"} {
    } elseif {[lindex $id 0] eq "item"} {
    } elseif {[lindex $id 0] eq "rowlabel"} {
	set row [lindex $id 1]
	if {$row ne $RowLabel(prev)} {
	    if {$RowLabel(prev) ne ""} {
		$w rowlabel state set $RowLabel(prev) !mouseover
	    }
	    $w rowlabel state set $row mouseover
	    set RowLabel(prev) $row
	}
	return
    }
    if {$RowLabel(prev) ne ""} {
	$w rowlabel state set $RowLabel(prev) !mouseover
	set RowLabel(prev) ""
    }
    return
}

