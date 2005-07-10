set RandomN 500
set RandomDepth 5

#
# Demo: random N items
#
proc DemoRandom {} {

	set T .f2.f1.t

	InitPics folder-* small-*

	set height [font metrics [$T cget -font] -linespace]
	if {$height < 18} {
		set height 18
	}

	#
	# Configure the treectrl widget
	#

	$T configure -itemheight $height -selectmode extended \
		-showroot yes -showrootbutton yes -showbuttons yes -showlines $::ShowLines \
		-scrollmargin 16 -xscrolldelay "500 50" -yscrolldelay "500 50"

	#
	# Create columns
	#

	$T column create -expand yes -text Item -itembackground {#e0e8f0 {}} -tag colItem
	$T column create -text Parent -justify center -itembackground {gray90 {}} -tag colParent
	$T column create -text Depth -justify center -itembackground {linen {}} -tag colDepth

	$T configure -treecolumn colItem

	#
	# Create elements
	#

	$T element create elemImgFolder image -image {folder-open {open} folder-closed {}}
	$T element create elemImgFile image -image small-file
	$T element create elemTxtName text \
		-fill [list $::SystemHighlightText {selected focus}]
	$T element create elemTxtCount text -fill blue
	$T element create elemTxtAny text
	$T element create elemRectSel rect -showfocus yes \
		-fill [list $::SystemHighlight {selected focus} gray {selected !focus}]

	#
	# Create styles using the elements
	#

	set S [$T style create styFolder]
	$T style elements $S {elemRectSel elemImgFolder elemTxtName elemTxtCount}
	$T style layout $S elemImgFolder -padx {0 4} -expand ns
	$T style layout $S elemTxtName -padx {0 4} -expand ns
	$T style layout $S elemTxtCount -padx {0 6} -expand ns
	$T style layout $S elemRectSel -union [list elemTxtName] -iexpand ns -ipadx 2

	set S [$T style create styFile]
	$T style elements $S {elemRectSel elemImgFile elemTxtName}
	$T style layout $S elemImgFile -padx {0 4} -expand ns
	$T style layout $S elemTxtName -padx {0 4} -expand ns
	$T style layout $S elemRectSel -union [list elemTxtName] -iexpand ns -ipadx 2

	set S [$T style create styAny]
	$T style elements $S {elemTxtAny}
	$T style layout $S elemTxtAny -padx 6 -expand ns

	TreeCtrl::SetSensitive $T {
		{colItem styFolder elemRectSel elemImgFolder elemTxtName}
		{colItem styFile elemRectSel elemImgFile elemTxtName}
	}
	TreeCtrl::SetDragImage $T {
		{colItem styFolder elemImgFolder elemTxtName}
		{colItem styFile elemImgFile elemTxtName}
	}

	#
	# Create items and assign styles
	#

	set clicks [clock clicks]
	set items [$T item create -count [expr {$::RandomN - 1}]]
	set added root
	foreach itemi $items {
		set j [expr {int(rand() * [llength $added])}]
		set itemj [lindex $added $j]
		if {[$T depth $itemj] < $::RandomDepth - 1} {
			lappend added $itemi
		}
		if {![$T item cget $itemj -button]} {
			$T item configure $itemj -button yes
		}
		if {rand() * 2 > 1} {
			$T item collapse $itemi
		}
		if {rand() * 2 > 1} {
			$T item lastchild $itemj $itemi
		} else {
			$T item firstchild $itemj $itemi
		}
	}
	puts "created $::RandomN-1 items in [expr [clock clicks] - $clicks] clicks"

	set clicks [clock clicks]
	lappend items [$T item id root]
	foreach item $items {
		set numChildren [$T item numchildren $item]
		if {$numChildren} {
			$T item style set $item colItem styFolder colParent styAny colDepth styAny
			$T item element configure $item \
				colItem elemTxtName -text "Item $item" + elemTxtCount -text "($numChildren)" , \
				colParent elemTxtAny -text "[$T item parent $item]" , \
				colDepth elemTxtAny -text "[$T depth $item]"
		} else {
			$T item style set $item colItem styFile colParent styAny colDepth styAny
			$T item element configure $item \
				colItem elemTxtName -text "Item $item" , \
				colParent elemTxtAny -text "[$T item parent $item]" , \
				colDepth elemTxtAny -text "[$T depth $item]"
		}
	}
	puts "configured $::RandomN items in [expr [clock clicks] - $clicks] clicks"

	bind DemoRandom <Double-ButtonPress-1> {
		TreeCtrl::DoubleButton1 %W %x %y
		break
	}
	bind DemoRandom <Control-ButtonPress-1> {
		set TreeCtrl::Priv(selectMode) toggle
		TreeCtrl::RandomButton1 %W %x %y
		break
	}
	bind DemoRandom <Shift-ButtonPress-1> {
		set TreeCtrl::Priv(selectMode) add
		TreeCtrl::RandomButton1 %W %x %y
		break
	}
	bind DemoRandom <ButtonPress-1> {
		set TreeCtrl::Priv(selectMode) set
		TreeCtrl::RandomButton1 %W %x %y
		break
	}
	bind DemoRandom <Button1-Motion> {
		TreeCtrl::RandomMotion1 %W %x %y
		break
	}
	bind DemoRandom <ButtonRelease-1> {
		TreeCtrl::RandomRelease1 %W %x %y
		break
	}

	bindtags $T [list $T DemoRandom TreeCtrl [winfo toplevel $T] all]

	return
}

proc TreeCtrl::RandomButton1 {T x y} {
	variable Priv
	focus $T
	set id [$T identify $x $y]
	set Priv(buttonMode) ""

	# Click outside any item
	if {$id eq ""} {
		$T selection clear

	# Click in header
	} elseif {[lindex $id 0] eq "header"} {
		ButtonPress1 $T $x $y

	# Click in item
	} else {
		foreach {where item arg1 arg2 arg3 arg4} $id {}
		switch $arg1 {
			button {
				$T item toggle $item
			}
			line {
				$T item toggle $arg2
			}
			column {
				set ok 0
				# Clicked an element
				if {[llength $id] == 6} {
					set column [lindex $id 3]
					set E [lindex $id 5]
					foreach list $Priv(sensitive,$T) {
						set C [lindex $list 0]
						set S [lindex $list 1]
						set eList [lrange $list 2 end]
						if {[$T column compare $column != $C]} continue
						if {[$T item style set $item $C] ne $S} continue
						if {[lsearch -exact $eList $E] == -1} continue
						set ok 1
						break
					}
				}
				if {!$ok} {
					$T selection clear
					return
				}

				set Priv(drag,motion) 0
				set Priv(drag,click,x) $x
				set Priv(drag,click,y) $y
					set Priv(drag,x) [$T canvasx $x]
				set Priv(drag,y) [$T canvasy $y]
				set Priv(drop) ""

				if {$Priv(selectMode) eq "add"} {
					BeginExtend $T $item
				} elseif {$Priv(selectMode) eq "toggle"} {
					BeginToggle $T $item
				} elseif {![$T selection includes $item]} {
					BeginSelect $T $item
				}
				$T activate $item

				if {[$T selection includes $item]} {
					set Priv(buttonMode) drag
				}
			}
		}
	}
	return
}

proc TreeCtrl::RandomMotion1 {T x y} {
	variable Priv
	switch $Priv(buttonMode) {
		"drag" {
			set Priv(autoscan,command,$T) {RandomMotion %T %x %y}
			AutoScanCheck $T $x $y
			RandomMotion $T $x $y
		}
		default {
			Motion1 $T $x $y
		}
	}
	return
}

proc TreeCtrl::RandomMotion {T x y} {
	variable Priv
	switch $Priv(buttonMode) {
		"drag" {
			if {!$Priv(drag,motion)} {
				# Detect initial mouse movement
				if {(abs($x - $Priv(drag,click,x)) <= 4) &&
					(abs($y - $Priv(drag,click,y)) <= 4)} return

				set Priv(selection) [$T selection get]
				set Priv(drop) ""
				$T dragimage clear
				# For each selected item, add 2nd and 3rd elements of
				# column "item" to the dragimage
				foreach I $Priv(selection) {
					foreach list $Priv(dragimage,$T) {
						set C [lindex $list 0]
						set S [lindex $list 1]
						if {[$T item style set $I $C] eq $S} {
							eval $T dragimage add $I $C [lrange $list 2 end]
						}
					}
				}
				set Priv(drag,motion) 1
			}

			# Find the item under the cursor
			set cursor X_cursor
			set drop ""
			set id [$T identify $x $y]
			set ok 0
			if {($id ne "") && ([lindex $id 0] eq "item") && ([llength $id] == 6)} {
				set item [lindex $id 1]
				set column [lindex $id 3]
				set E [lindex $id 5]
				foreach list $Priv(sensitive,$T) {
					set C [lindex $list 0]
					set S [lindex $list 1]
					set eList [lrange $list 2 end]
					if {[$T column compare $column != $C]} continue
					if {[$T item style set $item $C] ne $S} continue
					if {[lsearch -exact $eList $E] == -1} continue
					set ok 1
					break
				}
			}
			if {$ok} {
				# If the item is not in the pre-drag selection
				# (i.e. not being dragged) see if we can drop on it
				if {[lsearch -exact $Priv(selection) $item] == -1} {
					set drop $item
					# We can drop if dragged item isn't an ancestor
					foreach item2 $Priv(selection) {
						if {[$T item isancestor $item2 $item]} {
							set drop ""
							break
						}
					}
					if {$drop ne ""} {
						scan [$T item bbox $drop] "%d %d %d %d" x1 y1 x2 y2
						if {$y < $y1 + 3} {
							set cursor top_side
							set Priv(drop,pos) prevsibling
						} elseif {$y >= $y2 - 3} {
							set cursor bottom_side
							set Priv(drop,pos) nextsibling
						} else {
							set cursor ""
							set Priv(drop,pos) lastchild
						}
					}
				}
			}

			if {[$T cget -cursor] ne $cursor} {
				$T configure -cursor $cursor
			}

			# Select the item under the cursor (if any) and deselect
			# the previous drop-item (if any)
			$T selection modify $drop $Priv(drop)
			set Priv(drop) $drop

			# Show the dragimage in its new position
			set x [expr {[$T canvasx $x] - $Priv(drag,x)}]
			set y [expr {[$T canvasy $y] - $Priv(drag,y)}]
			$T dragimage offset $x $y
			$T dragimage configure -visible yes
		}
		default {
			Motion1 $T $x $y
		}
	}
	return
}

proc TreeCtrl::RandomRelease1 {T x y} {
	variable Priv
    if {![info exists Priv(buttonMode)]} return
	switch $Priv(buttonMode) {
		"drag" {
			AutoScanCancel $T
			$T dragimage configure -visible no
			$T selection modify {} $Priv(drop)
			$T configure -cursor ""
			if {$Priv(drop) ne ""} {
				RandomDrop $T $Priv(drop) $Priv(selection) $Priv(drop,pos)
			}
			unset Priv(buttonMode)
		}
		default {
			Release1 $T $x $y
		}
	}
	return
}

proc RandomDrop {T target source pos} {
	set parentList {}
	switch -- $pos {
		lastchild { set parent $target }
		prevsibling { set parent [$T item parent $target] }
		nextsibling { set parent [$T item parent $target] }
	}
	foreach item $source {

		# Ignore any item whose ancestor is also selected
		set ignore 0
		foreach ancestor [$T item ancestors $item] {
			if {[lsearch -exact $source $ancestor] != -1} {
				set ignore 1
				break
			}
		}
		if {$ignore} continue

		# Update the old parent of this moved item later
		if {[lsearch -exact $parentList $item] == -1} {
			lappend parentList [$T item parent $item]
		}

		# Add to target
		$T item $pos $target $item

		# Update text: parent
		$T item element configure $item colParent elemTxtAny -text $parent

		# Update text: depth
		$T item element configure $item colDepth elemTxtAny -text [$T depth $item]

		# Recursively update text: depth
		set itemList [$T item firstchild $item]
		while {[llength $itemList]} {
			# Pop
			set item [lindex $itemList end]
			set itemList [lrange $itemList 0 end-1]

			$T item element configure $item colDepth elemTxtAny -text [$T depth $item]

			set item2 [$T item nextsibling $item]
			if {$item2 ne ""} {
				# Push
				lappend itemList $item2
			}
			set item2 [$T item firstchild $item]
			if {$item2 ne ""} {
				# Push
				lappend itemList $item2
			}
		}
	}

	# Update items that lost some children
	foreach item $parentList {
		set numChildren [$T item numchildren $item]
		if {$numChildren == 0} {
			$T item configure $item -button no
			$T item style map $item colItem styFile {elemTxtName elemTxtName}
		} else {
			$T item element configure $item colItem elemTxtCount -text "($numChildren)"
		}
	}

	# Update the target that gained some children
	if {[$T item style set $parent colItem] ne "styFolder"} {
		$T item configure $parent -button yes
		$T item style map $parent colItem styFolder {elemTxtName elemTxtName}
	}
	set numChildren [$T item numchildren $parent]
	$T item element configure $parent colItem elemTxtCount -text "($numChildren)"
	return
}

#
# Demo: random N items, button images
#
proc DemoRandom2 {} {

	set T .f2.f1.t

	DemoRandom

	InitPics mac-*

	$T configure -buttonimage {mac-collapse open mac-expand {}} \
		-showlines no

	return
}

