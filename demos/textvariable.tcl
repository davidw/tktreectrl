proc DemoTextvariable {} {

	set T .f2.f1.t

	#
	# Configure the treectrl widget
	#

	$T configure -showroot no -showbuttons no -showlines no \
		-selectmode extended -xscrollincrement 20 -showheader no

	# Hide the borders because child windows appear on top of them
	$T configure -borderwidth 0 -highlightthickness 0

	#
	# Create columns
	#

	$T column create -expand yes -tag C0
	$T configure -treecolumn C0

	#
	# Create elements
	#

	$T element create eWindow window
	$T element create eRect rect
	$T element create eText1 text -width 300
	$T element create eText2 text

	#
	# Create styles using the elements
	#

	set S [$T style create s1 -orient horizontal]
	$T style elements $S eText1
	$T style layout $S eText1 -padx 10 -pady 6

	set S [$T style create s2 -orient vertical]
	$T style elements $S {eRect eText2 eWindow}
	$T style layout $S eRect -union {eText2 eWindow} -ipadx 8 -ipady 8 -padx 4 -pady {0 4}
	$T style layout $S eText2 -pady {0 6}
	$T style layout $S eWindow

	#
	# Create items and assign styles
	#

	set I [$T item create]
	$T item style set $I C0 s1
	$T item element configure $I C0 eText1 -text "Each text element and entry widget share the same -textvariable. Editing the text in the entry automatically updates the text element."
	$T item lastchild root $I

	foreach i {0 1} color {gray75 "light blue"} {
		set I [$T item create]
		$T item style set $I C0 s2
		set e [entry $T.e$I -width 48 -textvariable tvar$I]
		$T item element configure $I C0 \
			eRect -fill [list $color] + \
			eText2 -textvariable tvar$I + \
			eWindow -window $e
		$T item lastchild root $I
		set ::tvar$I "This is item $I"
	}

	return
}

