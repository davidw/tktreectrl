# RCS: @(#) $Id: column-lock.tcl,v 1.3 2006/10/28 01:27:45 treectrl Exp $

proc DemoColumnLock {} {

    global ColumnLock

    set T .f2.f1.t

    InitPics *checked

    #
    # Configure the treectrl widget
    #

    $T configure \
	-showbuttons no \
	-showlines no \
	-showroot no \
	-xscrollincrement 6 -xscrolldelay 30 \
	-yscrollincrement 6 -yscrolldelay 30

    #
    # Create columns
    #

    for {set i 0} {$i < 100} {incr i} {
	$T column create -text "C$i" -tags C$i -width [expr {40 + 20 * ($i % 2)}] -justify center
    }
    $T column configure first -text LEFT -lock left -width ""
    $T column configure last -text RIGHT -lock right -width ""

    $T state define CHECK
    $T state define mouseover

    #
    # Create styles for the left-locked column, and create items
    #

    $T element create label1.bg rect -fill {gray80 mouseover gray {}}
    $T element create label1.text text
    $T style create label1 -orient horizontal
    $T style elements label1 {label1.bg label1.text}
    $T style layout label1 label1.bg -detach yes -iexpand xy
    $T style layout label1 label1.text -expand wns -padx 2
    
    for {set i 1} {$i <= 10} {incr i} {
	set I [$T item create -tags R$i -parent root]
	$T item style set $I C0 label1
	$T item text $I C0 "R$i"
    }

    $T element create label2.bd border -background $::SystemButtonFace \
	-relief raised -thickness 2 -filled yes
    $T element create label2.text text
    $T style create label2 -orient horizontal
    $T style elements label2 {label2.bd label2.text}
    $T style layout label2 label2.bd -detach yes -iexpand xy
    $T style layout label2 label2.text -expand news -padx 2 -pady 2
    
    for {set i 11} {$i <= 20} {incr i} {
	set I [$T item create -tags R$i -parent root]
	$T item style set $I C0 label2
	$T item text $I C0 "R$i"
    }

    $T element create label3.div rect -fill black -height 2
    $T element create label3.text text
    $T style create label3 -orient horizontal
    $T style elements label3 {label3.div label3.text}
    $T style layout label3 label3.div -detach yes -expand n -iexpand x
    $T style layout label3 label3.text -expand ws -padx 2 -pady 2
    
    for {set i 21} {$i <= 30} {incr i} {
	set I [$T item create -tags R$i -parent root]
	$T item style set $I C0 label3
	$T item text $I C0 "R$i"
    }

    $T element create label4.rect rect -fill {#e0e8f0 mouseover}
    $T element create label4.text text
    $T element create label4.w window -clip yes -destroy yes
    $T style create label4 -orient vertical
    $T style elements label4 {label4.rect label4.text label4.w}
    $T style layout label4 label4.rect -detach yes -iexpand xy
    $T style layout label4 label4.text -expand we -padx 2 -pady 2
    $T style layout label4 label4.w -iexpand x -padx 2 -pady {0 2}
    
    for {set i 31} {$i <= 40} {incr i} {
	set I [$T item create -tags R$i -parent root]
	$T item style set $I C0 label4
	$T item element configure $I C0 label4.text -textvariable ::DemoColumnLock(R$i)
	set clip [frame $T.clipR${I}C0 -borderwidth 0]
	$::entryCmd $clip.e -width 4 -textvariable ::DemoColumnLock(R$i)
	$T item element configure $I C0 label4.w -window $clip
	set ::DemoColumnLock(R$i) "R$i"
    }

    #
    # Create styles for the right-locked column
    #

    $T element create labelR1.bg rect -fill {gray80 mouseover gray {}}
    $T element create labelR1.img image -image {checked CHECK unchecked {}}
    $T style create labelR1 -orient horizontal
    $T style elements labelR1 {labelR1.bg labelR1.img}
    $T style layout labelR1 labelR1.bg -detach yes -iexpand xy
    $T style layout labelR1 labelR1.img -expand news -padx 2 -pady 2

    $T element create labelR2.bd border -background $::SystemButtonFace \
	-relief raised -thickness 2 -filled yes
    $T element create labelR2.img image -image {checked CHECK unchecked {}}
    $T style create labelR2 -orient horizontal
    $T style elements labelR2 {labelR2.bd labelR2.img}
    $T style layout labelR2 labelR2.bd -detach yes -iexpand xy
    $T style layout labelR2 labelR2.img -expand news -padx 2 -pady 2

    $T element create labelR3.div rect -fill black -height 2
    $T element create labelR3.img image -image {checked CHECK unchecked {}}
    $T style create labelR3 -orient horizontal
    $T style elements labelR3 {labelR3.div labelR3.img}
    $T style layout labelR3 labelR3.div -detach yes -expand n -iexpand x
    $T style layout labelR3 labelR3.img -expand news -padx 2 -pady 2

    $T element create labelR4.rect rect -fill {#e0e8f0 mouseover}
    $T element create labelR4.img image -image {checked CHECK unchecked {}}
    $T style create labelR4 -orient vertical
    $T style elements labelR4 {labelR4.rect labelR4.img}
    $T style layout labelR4 labelR4.rect -detach yes -iexpand xy
    $T style layout labelR4 labelR4.img -expand news -padx 2 -pady 2

    $T item style set {range 1 10} last labelR1
    $T item style set {range 11 20} last labelR2
    $T item style set {range 21 30} last labelR3
    $T item style set {range 31 40} last labelR4

    #
    # Create styles for the non-locked columns
    #

    $T element create eRect rect -outline gray -outlinewidth 1 -open wn \
	-fill {gray80 mouseover}
    $T style create s1 -orient horizontal
    $T style elements s1 eRect
    $T style layout s1 eRect -detach yes -iexpand xy

    $T item style set "root children" "range {first next} {last prev}" s1

    $T element create windowStyle.rect rect -fill {#e0e8f0 mouseover}
    $T element create windowStyle.text text
    $T element create windowStyle.w window -clip yes -destroy yes
    $T style create windowStyle -orient vertical
    $T style elements windowStyle {windowStyle.rect windowStyle.text windowStyle.w}
    $T style layout windowStyle windowStyle.rect -detach yes -iexpand xy
    $T style layout windowStyle windowStyle.text -expand we -padx 2 -pady 2
    $T style layout windowStyle windowStyle.w -iexpand x -padx 2 -pady {0 2}

    $T item style set "list {2 22}" "all lock none" windowStyle

    foreach C [lrange [$T column list] 1 end-1] {
	set ::DemoColumnLock(C$C) "C$C"

	set I 2
	set clip [frame $T.clipR${I}C$C -borderwidth 0]
	$::entryCmd $clip.e -width 4 -textvariable ::DemoColumnLock(C$C)
	$T item element configure $I $C windowStyle.w -window $clip + \
	    windowStyle.text -textvariable ::DemoColumnLock(C$C)

	set I 22
	set clip [frame $T.clipR${I}C$C -borderwidth 0]
	$::entryCmd $clip.e -width 4 -textvariable ::DemoColumnLock(C$C)
	$T item element configure $I $C windowStyle.w -window $clip + \
	    windowStyle.text -textvariable ::DemoColumnLock(C$C)
    }

    bind DemoColumnLock <ButtonPress-1> {
	set id [%W identify %x %y]
	if {[lindex $id 0] eq "item"} {
	    foreach {what item where arg1 arg2 arg3} $id {}
	    if {$where eq "column"} {
		if {[%W column compare $arg1 == last]} {
		    %W item state forcolumn $item $arg1 ~CHECK
		}
	    }
	}
    }

    bind DemoColumnLock <Button1-Motion> {
	# no-op
    }
    bind DemoColumnLock <Motion> {
	ColumnLockMotion %W %x %y
    }

    set ColumnLock(prev) ""
    bindtags $T [list $T DemoColumnLock TreeCtrl [winfo toplevel $T] all]

    return
}

proc ColumnLockMotion {w x y} {
    global ColumnLock
    set id [$w identify $x $y]
    if {$id eq ""} {
    } elseif {[lindex $id 0] eq "header"} {
    } elseif {[lindex $id 0] eq "item"} {
	set item [lindex $id 1]
	if {[lindex $id 2] eq "column"} {
	    set column [lindex $id 3]
	} else {
	    set column [$w cget -treecolumn]
	}
	if {[list $item $column] ne $ColumnLock(prev)} {
	    if {$ColumnLock(prev) ne ""} {
		eval $w item state forcolumn $ColumnLock(prev) !mouseover
	    }
	    $w item state forcolumn $item $column mouseover
	    set ColumnLock(prev) [list $item $column]
	}
	return
    }
    if {$ColumnLock(prev) ne ""} {
	eval $w item state forcolumn $ColumnLock(prev) !mouseover
	set ColumnLock(prev) ""
    }
    return
}

