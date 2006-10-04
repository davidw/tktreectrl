# RCS: @(#) $Id: treectrl.tcl,v 1.26 2006/10/04 03:54:25 treectrl Exp $

bind TreeCtrl <Motion> {
    TreeCtrl::CursorCheck %W %x %y
    TreeCtrl::MotionInHeader %W %x %y
}
bind TreeCtrl <Leave> {
    TreeCtrl::CursorCancel %W
    TreeCtrl::MotionInHeader %W
}
bind TreeCtrl <ButtonPress-1> {
    TreeCtrl::ButtonPress1 %W %x %y
}
bind TreeCtrl <Double-ButtonPress-1> {
    TreeCtrl::DoubleButton1 %W %x %y
}
bind TreeCtrl <Button1-Motion> {
    TreeCtrl::Motion1 %W %x %y
}
bind TreeCtrl <ButtonRelease-1> {
    TreeCtrl::Release1 %W %x %y
}
bind TreeCtrl <Shift-ButtonPress-1> {
    set TreeCtrl::Priv(buttonMode) normal
    TreeCtrl::BeginExtend %W [%W item id {nearest %x %y}]
}
# Command-click should provide a discontinuous selection on OSX
switch -- [tk windowingsystem] {
    "aqua" { set modifier Command }
    default { set modifier Control }
}
bind TreeCtrl <$modifier-ButtonPress-1> {
    set TreeCtrl::Priv(buttonMode) normal
    TreeCtrl::BeginToggle %W [%W item id {nearest %x %y}]
}
bind TreeCtrl <Button1-Leave> {
    TreeCtrl::Leave1 %W %x %y
}
bind TreeCtrl <Button1-Enter> {
    TreeCtrl::Enter1 %W %x %y
}

bind TreeCtrl <KeyPress-Up> {
    TreeCtrl::SetActiveItem %W [TreeCtrl::UpDown %W active -1]
}
bind TreeCtrl <Shift-KeyPress-Up> {
    TreeCtrl::Extend %W above
}
bind TreeCtrl <KeyPress-Down> {
    TreeCtrl::SetActiveItem %W [TreeCtrl::UpDown %W active 1]
}
bind TreeCtrl <Shift-KeyPress-Down> {
    TreeCtrl::Extend %W below
}
bind TreeCtrl <KeyPress-Left> {
    TreeCtrl::SetActiveItem %W [TreeCtrl::LeftRight %W active -1]
}
bind TreeCtrl <Shift-KeyPress-Left> {
    TreeCtrl::Extend %W left
}
bind TreeCtrl <Control-KeyPress-Left> {
    %W xview scroll -1 pages
}
bind TreeCtrl <KeyPress-Right> {
    TreeCtrl::SetActiveItem %W [TreeCtrl::LeftRight %W active 1]
}
bind TreeCtrl <Shift-KeyPress-Right> {
    TreeCtrl::Extend %W right
}
bind TreeCtrl <Control-KeyPress-Right> {
    %W xview scroll 1 pages
}
bind TreeCtrl <KeyPress-Prior> {
    %W yview scroll -1 pages
    if {[%W item id {nearest 0 0}] ne ""} {
	%W activate {nearest 0 0}
    }
}
bind TreeCtrl <KeyPress-Next> {
    %W yview scroll 1 pages
    if {[%W item id {nearest 0 0}] ne ""} {
	%W activate {nearest 0 0}
    }
}
bind TreeCtrl <Control-KeyPress-Prior> {
    %W xview scroll -1 pages
}
bind TreeCtrl <Control-KeyPress-Next> {
    %W xview scroll 1 pages
}
bind TreeCtrl <KeyPress-Home> {
    %W xview moveto 0
}
bind TreeCtrl <KeyPress-End> {
    %W xview moveto 1
}
bind TreeCtrl <Control-KeyPress-Home> {
    TreeCtrl::SetActiveItem %W [%W item id {first visible state enabled}]
}
bind TreeCtrl <Shift-Control-KeyPress-Home> {
    TreeCtrl::DataExtend %W [%W item id {first visible state enabled}]
}
bind TreeCtrl <Control-KeyPress-End> {
    TreeCtrl::SetActiveItem %W [%W item id {last visible state enabled}]
}
bind TreeCtrl <Shift-Control-KeyPress-End> {
    TreeCtrl::DataExtend %W [%W item id {last visible state enabled}]
}
bind TreeCtrl <<Copy>> {
    if {[string equal [selection own -displayof %W] "%W"]} {
	clipboard clear -displayof %W
	clipboard append -displayof %W [selection get -displayof %W]
    }
}
bind TreeCtrl <KeyPress-space> {
    TreeCtrl::BeginSelect %W [%W item id active]
}
bind TreeCtrl <KeyPress-Select> {
    TreeCtrl::BeginSelect %W [%W item id active]
}
bind TreeCtrl <Control-Shift-KeyPress-space> {
    TreeCtrl::BeginExtend %W [%W item id active]
}
bind TreeCtrl <Shift-KeyPress-Select> {
    TreeCtrl::BeginExtend %W [%W item id active]
}
bind TreeCtrl <KeyPress-Escape> {
    TreeCtrl::Cancel %W
}
bind TreeCtrl <Control-KeyPress-slash> {
    TreeCtrl::SelectAll %W
}
bind TreeCtrl <Control-KeyPress-backslash> {
    if {[string compare [%W cget -selectmode] "browse"]} {
	%W selection clear
    }
}

bind TreeCtrl <KeyPress-plus> {
    %W item expand [%W item id active]
}
bind TreeCtrl <KeyPress-minus> {
    %W item collapse [%W item id active]
}
bind TreeCtrl <KeyPress-Return> {
    %W item toggle [%W item id active]
}


# Additional Tk bindings that aren't part of the Motif look and feel:

bind TreeCtrl <ButtonPress-2> {
    TreeCtrl::ScanMark %W %x %y
}
bind TreeCtrl <Button2-Motion> {
    TreeCtrl::ScanDrag %W %x %y
}

if {$tcl_platform(platform) eq "windows"} {
    bind TreeCtrl <Control-ButtonPress-3> {
	TreeCtrl::ScanMark %W %x %y
    }
    bind TreeCtrl <Control-Button3-Motion> {
	TreeCtrl::ScanDrag %W %x %y
    }
}

# MouseWheel
if {[string equal "x11" [tk windowingsystem]]} {
    # Support for mousewheels on Linux/Unix commonly comes through mapping
    # the wheel to the extended buttons.  If you have a mousewheel, find
    # Linux configuration info at:
    #	http://www.inria.fr/koala/colas/mouse-wheel-scroll/
    bind TreeCtrl <4> {
	if {!$tk_strictMotif} {
	    %W yview scroll -5 units
	}
    }
    bind TreeCtrl <5> {
	if {!$tk_strictMotif} {
	    %W yview scroll 5 units
	}
    }
} elseif {[string equal [tk windowingsystem] "aqua"]} {
    bind TreeCtrl <MouseWheel> {
	%W yview scroll [expr {- (%D)}] units
    }
} else {
    bind TreeCtrl <MouseWheel> {
	%W yview scroll [expr {- (%D / 120) * 4}] units
    }
}

namespace eval ::TreeCtrl {
    variable Priv
    array set Priv {
	prev {}
    }
}

# Retrieve filelist bindings from this dir
source [file join [file dirname [info script]] filelist-bindings.tcl]

# ::TreeCtrl::CursorAction --
#
# If the given point is at the left or right edge of a resizable column, the
# result is "column C". If the given point is at the right edge of a rowlabel,
# the result is "rowlabel all". If the given point is at the top or bottom
# edge of a resizable rowlabel, the result is "rowlabel R".
#
# Arguments:
# w		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord or pointer.

proc ::TreeCtrl::CursorAction {w x y} {
    variable Priv
    set id [$w identify $x $y]
    if {([llength $id] == 3) && ([lindex $id 0] eq "header")} {
	set column [lindex $id 1]
	set side [lindex $id 2]
	if {$side eq "left"} {
	    if {[$w column compare $column != "first visible"]} {
		set column [$w column id "$column prev visible"]
		if {[$w column cget $column -resize]} {
		    return "column $column"
		}
	    }
	} else {
	    if {[$w column cget $column -resize]} {
		return "column $column"
	    }
	}
    }
    scan [$w contentbox] "%d %d %d %d" left top right bottom
    if {[$w cget -showrowlabels] && [$w cget -rowlabelresize] &&
	    ($x >= $left - 4) && ($x < $left + 4)} {
	if {![$w cget -showheader] || ($y < $top)} {
	    return "rowlabel all"
	}
    }
    if {([llength $id] > 1) && ([lindex $id 0] eq "rowlabel")} {
	set rowlabel [lindex $id 1]
	if {[lindex $id end] eq "top"} {
	    if {[$w rowlabel compare $rowlabel != "first visible"]} {
		set rowlabel [$w rowlabel id "$rowlabel prev visible"]
		if {[$w rowlabel cget $rowlabel -resize]} {
		    return "rowlabel $rowlabel"
		}
	    }
	}
	if {[lindex $id end] eq "bottom"} {
	    if {[$w rowlabel cget $rowlabel -resize]} {
		return "rowlabel $rowlabel"
	    }
	}
    }
    return ""
}

# ::TreeCtrl::CursorCheck --
#
# Sees if the given pointer coordinates are near the edge of a resizable
# column in the header. If so and the treectrl's cursor is not already
# set to sb_h_double_arrow, then the current cursor is saved and changed
# to sb_h_double_arrow, and an [after] callback to CursorCheckAux is
# scheduled.
#
# Arguments:
# w		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord or pointer.

proc ::TreeCtrl::CursorCheck {w x y} {
    variable Priv
    set action [CursorAction $w $x $y]
    if {$action eq ""} {
	CursorCancel $w
	return
    }
    if {[lindex $action 0] eq "column"} {
	set cursor sb_h_double_arrow
    }
    if {[lindex $action 0] eq "rowlabel"} {
	set rowlabel [lindex $action 1]
	if {$rowlabel eq "all"} {
	    set cursor sb_h_double_arrow
	} else {
	    set cursor sb_v_double_arrow
	}
    }
    if {![info exists Priv(cursor,$w)]} {
	set Priv(cursor,$w) [$w cget -cursor]
    }
    $w configure -cursor $cursor
    if {[info exists Priv(cursor,afterId,$w)]} {
	after cancel $Priv(cursor,afterId,$w)
    }
    set Priv(cursor,afterId,$w) [after 150 [list TreeCtrl::CursorCheckAux $w]]
    return
}

# ::TreeCtrl::CursorCheckAux --
#
# Get's the location of the pointer and calls CursorCheck if the treectrl's
# cursor was previously set to sb_h_double_arrow.
#
# Arguments:
# w		The treectrl widget.

proc ::TreeCtrl::CursorCheckAux {w} {
    variable Priv
    set x [winfo pointerx $w]
    set y [winfo pointery $w]
    if {[info exists Priv(cursor,$w)]} {
	set x [expr {$x - [winfo rootx $w]}]
	set y [expr {$y - [winfo rooty $w]}]
	CursorCheck $w $x $y
    }
    return
}

# ::TreeCtrl::CursorCancel --
#
# Restores the treectrl's cursor if it was changed to sb_h_double_arrow.
# Cancels any pending [after] callback to CursorCheckAux.
#
# Arguments:
# w		The treectrl widget.

proc ::TreeCtrl::CursorCancel {w} {
    variable Priv
    if {[info exists Priv(cursor,$w)]} {
	$w configure -cursor $Priv(cursor,$w)
	unset Priv(cursor,$w)
    }
    if {[info exists Priv(cursor,afterId,$w)]} {
	after cancel $Priv(cursor,afterId,$w)
	unset Priv(cursor,afterId,$w)
    }
    return
}

# ::TreeCtrl::MotionInHeader --
#
# This procedure updates the active/normal states of columns as the pointer
# moves in and out of column headers. Typically this results in visual
# feedback by changing the appearance of the headers.
#
# Arguments:
# w		The treectrl widget.
# args		x y coords if the pointer is in the window, or an empty list.

proc ::TreeCtrl::MotionInHeader {w args} {
    variable Priv
    if {[llength $args]} {
	set x [lindex $args 0]
	set y [lindex $args 1]
	set id [$w identify $x $y]
    } else {
	set id ""
    }
    if {[info exists Priv(inheader,$w)]} {
	set prevColumn $Priv(inheader,$w)
    } else {
	set prevColumn ""
    }
    set column ""
    if {[lindex $id 0] eq "header"} {
	set column [lindex $id 1]
	if {[lindex $id 2] eq "left"} {
	    if {[$w column compare $column != "first visible"]} {
		set column [$w column id "$column prev visible"]
	    }
	}
	if {[$w column compare $column == "tail"]} {
	    set column ""
	} elseif {![$w column cget $column -button]} {
	    set column ""
	}
    }
    if {$column ne $prevColumn} {
	if {$prevColumn ne ""} {
	    $w column configure $prevColumn -state normal
	}
	if {$column ne ""} {
	    $w column configure $column -state active
	    set Priv(inheader,$w) $column
	} else {
	    unset Priv(inheader,$w)
	}
    }
    return
}

# ::TreeCtrl::ButtonPress1 --
#
# Handle <ButtonPress-1> event.
#
# Arguments:
# w		The treectrl widget.
# x		Window x coord.
# y		Window y coord.

proc ::TreeCtrl::ButtonPress1 {w x y} {
    variable Priv
    focus $w

    set action [CursorAction $w $x $y]
    if {$action ne "" && [lindex $action 0] eq "rowlabel"} {
	set Priv(buttonMode) rowlabel
	set Priv(rowlabel) [lindex $action 1]
	set Priv(x) $x
	set Priv(y) $y
	set Priv(width) [$w rowlabel width]
	if {$Priv(rowlabel) ne "all"} {
	    scan [$w rowlabel bbox $Priv(rowlabel)] "%d %d %d %d" x1 y1 x2 y2
	    set Priv(height) [expr {$y2 - $y1}]
	}
	return
    }

    set id [$w identify $x $y]
    if {$id eq ""} {
	return
    }
    if {[lindex $id 0] eq "item"} {
	foreach {where item arg1 arg2} $id {}
	if {$arg1 eq "button"} {
	    $w item toggle $item
	    return
	} elseif {$arg1 eq "line"} {
	    $w item toggle $arg2
	    return
	}
    }
    set Priv(buttonMode) ""
    if {[lindex $id 0] eq "header"} {
	set column [lindex $id 1]
	if {[llength $id] == 3} {
	    set side [lindex $id 2]
	    if {$side eq "left"} {
		if {[$w column compare $column == "first visible"]} return
		set column [$w column id "$column prev visible"]
	    }
	    if {![$w column cget $column -resize]} return
	    set Priv(buttonMode) resize
	    set Priv(column) $column
	    set Priv(x) $x
	    set Priv(y) $y
	    set Priv(width) [$w column width $column]
	    return
	}
	if {[$w column compare $column == "tail"]} return
	if {![$w column cget $column -button]} {
	    if {![$w column dragcget -enable]} return
	    set Priv(buttonMode) dragColumnWait
	} else {
	    set Priv(buttonMode) header
	    $w column configure $column -state pressed
	}
	set Priv(column) $column
	set Priv(columnDrag,x) $x
	set Priv(columnDrag,y) $y
	return
    }
    if {[lindex $id 0] eq "rowlabel"} {
	# What action should be taken when clicking in a rowlabel?
	# Resizing is handled above.
	return
    }
    set item [lindex $id 1]
    if {![$w item enabled $item]} {
	return
    }
    set Priv(buttonMode) normal
    BeginSelect $w $item
    return
}

# ::TreeCtrl::DoubleButtonPress1 --
#
# Handle <Double-ButtonPress-1> event.
#
# Arguments:
# w		The treectrl widget.
# x		Window x coord.
# y		Window y coord.

proc ::TreeCtrl::DoubleButton1 {w x y} {

    set action [CursorAction $w $x $y]
    if {$action ne "" && [lindex $action 0] eq "rowlabel"} {
	set rowlabel [lindex $action 1]
	if {$rowlabel eq "all"} {
	    $w configure -rowlabelwidth ""
	} else {
	    $w rowlabel configure $rowlabel -height ""
	}
    }

    set id [$w identify $x $y]
    if {$id eq ""} {
	return
    }
    if {[lindex $id 0] eq "item"} {
	foreach {where item arg1 arg2} $id {}
	if {$arg1 eq "button"} {
	    $w item toggle $item
	    return
	} elseif {$arg1 eq "line"} {
	    $w item toggle $arg2
	    return
	}
    }
    if {[lindex $id 0] eq "header"} {
	set column [lindex $id 1]
	# Double-click between columns to set default column width
	if {[llength $id] == 3} {
	    set side [lindex $id 2]
	    if {$side eq "left"} {
		if {[$w column compare $column == "first visible"]} return
		set column [$w column id "$column prev visible"]
	    }
	    if {[$w column compare $column == "tail"]} return
	    $w column configure $column -width ""
	    CursorCheck $w $x $y
	    MotionInHeader $w $x $y
	} else {
	    ButtonPress1 $w $x $y
	}
    }
    return
}

# ::TreeCtrl::Motion1 --
#
# Handle <Button1-Motion> event.
#
# Arguments:
# w		The treectrl widget.
# x		Window x coord.
# y		Window y coord.

proc ::TreeCtrl::Motion1 {w x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	header {
	    set id [$w identify $x $y]
	    if {![string match "header $Priv(column)*" $id]} {
		if {[$w column cget $Priv(column) -state] eq "pressed"} {
		    $w column configure $Priv(column) -state normal
		}
	    } else {
		if {[$w column cget $Priv(column) -state] ne "pressed"} {
		    $w column configure $Priv(column) -state pressed
		}
		if {[$w column dragcget -enable] &&
		    (abs($Priv(columnDrag,x) - $x) > 4)} {
		    $w column dragconfigure \
			-imagecolumn $Priv(column) \
			-imageoffset [expr {$x - $Priv(columnDrag,x)}]
		    set Priv(buttonMode) dragColumn
		    TryEvent $w ColumnDrag begin [list C $Priv(column)]
		}
	    }
	}
	dragColumnWait {
	    if {(abs($Priv(columnDrag,x) - $x) > 4)} {
		$w column dragconfigure \
		    -imagecolumn $Priv(column) \
		    -imageoffset [expr {$x - $Priv(columnDrag,x)}]
		set Priv(buttonMode) dragColumn
		TryEvent $w ColumnDrag begin [list C $Priv(column)]
	    }
	}
	dragColumn {
	    scan [$w column bbox $Priv(column)] "%d %d %d %d" x1 y1 x2 y2
	    if {$y < $y1 - 30 || $y >= $y2 + 30} {
		set inside 0
	    } else {
		set inside 1
	    }
	    if {$inside && ([$w column dragcget -imagecolumn] eq "")} {
		$w column dragconfigure -imagecolumn $Priv(column)
	    } elseif {!$inside && ([$w column dragcget -imagecolumn] ne "")} {
		$w column dragconfigure -imagecolumn "" -indicatorcolumn ""
	    }
	    if {$inside} {
		$w column dragconfigure -imageoffset [expr {$x - $Priv(columnDrag,x)}]
		set id [$w identify $x $Priv(columnDrag,y)]
		if {[lindex $id 0] eq "header"} {
		    set column [lindex $id 1]
		    if {[$w column compare $column == "tail"]} {
			$w column dragconfigure -indicatorcolumn tail
		    } elseif {$column ne ""} {
			scan [$w column bbox $column] "%d %d %d %d" x1 y1 x2 y2
			if {$x < $x1 + ($x2 - $x1) / 2} {
			    $w column dragconfigure -indicatorcolumn $column
			} else {
			    $w column dragconfigure -indicatorcolumn "$column next visible"
			}
		    }
		}
	    }
	    ColumnDragScrollCheck $w $x $y
	}
	normal {
	    set Priv(x) $x
	    set Priv(y) $y
	    SelectionMotion $w [$w item id [list nearest $x $y]]
	    set Priv(autoscan,command,$w) {SelectionMotion %T [%T item id "nearest %x %y"]}
	    AutoScanCheck $w $x $y
	}
	resize {
	    set width [expr {$Priv(width) + $x - $Priv(x)}]
	    set minWidth [$w column cget $Priv(column) -minwidth]
	    set maxWidth [$w column cget $Priv(column) -maxwidth]
	    if {$minWidth eq ""} {
		set minWidth 0
	    }
	    if {$width < $minWidth} {
		set width $minWidth
	    }
	    if {($maxWidth ne "") && ($width > $maxWidth)} {
		set width $maxWidth
	    }
	    if {$width == 0} {
		incr width
	    }
	    switch -- [$w cget -columnresizemode] {
		proxy {
		    scan [$w column bbox $Priv(column)] "%d %d %d %d" x1 y1 x2 y2
		    # Use "ne" because -columnproxy could be ""
		    if {($x1 + $width - 1) ne [$w cget -columnproxy]} {
			$w configure -columnproxy [expr {$x1 + $width - 1}]
		    }
		}
		realtime {
		    if {[$w column cget $Priv(column) -width] != $width} {
			$w column configure $Priv(column) -width $width
		    }
		}
	    }
	}
	rowlabel {
	    if {$Priv(rowlabel) eq "all"} {
		set width [expr {$Priv(width) + $x - $Priv(x)}]
		set minWidth [$w cget -minrowlabelwidth]
		set maxWidth [$w cget -maxrowlabelwidth]
		if {$minWidth eq ""} {
		    set minWidth 0
		}
		if {$minWidth >= 0 && $width < $minWidth} {
		    set width $minWidth
		}
		if {$maxWidth ne "" && $width > $maxWidth} {
		    set width $maxWidth
		}
		if {$width == 0} {
		    incr width
		}
		switch -- [$w cget -columnresizemode] {
		    proxy {
			scan [$w contentbox] "%d %d %d %d" x1 y1 x2 y2
			incr x1 -[$w rowlabel width]
			# Use "ne" because -columnproxy could be ""
			if {($x1 + $width - 1) ne [$w cget -columnproxy]} {
			    $w configure -columnproxy [expr {$x1 + $width - 1}]
			}
		    }
		    realtime {
			if {[$w cget -rowlabelwidth] != $width} {
			    $w configure -rowlabelwidth $width
			}
		    }
		}
	    } else {
		set height [expr {$Priv(height) + $y - $Priv(y)}]
		set minHeight [$w rowlabel cget $Priv(rowlabel) -minheight]
		set maxHeight [$w rowlabel cget $Priv(rowlabel) -maxheight]
		if {$minHeight eq ""} {
		    set minHeight 0
		}
		if {$minHeight >= 0 && $height < $minHeight} {
		    set height $minHeight
		}
		if {$maxHeight ne "" && $height > $maxHeight} {
		    set height $maxHeight
		}
		if {$height == 0} {
		    incr height
		}
		switch -- [$w cget -columnresizemode] {
		    proxy {
			scan [$w rowlabel bbox $Priv(rowlabel)] "%d %d %d %d" x1 y1 x2 y2
			# Use "ne" because -rowproxy could be ""
			if {($y1 + $height - 1) ne [$w cget -rowproxy]} {
			    $w configure -rowproxy [expr {$y1 + $height - 1}]
			}
		    }
		    realtime {
			if {[$w rowlabel cget $Priv(rowlabel) -height] != $height} {
			    $w rowlabel configure $Priv(rowlabel) -height $height
			}
		    }
		}
	    }
	}
    }
    return
}

# ::TreeCtrl::Leave1 --
#
# Handle <Button1-Leave> event.
#
# Arguments:
# w		The treectrl widget.
# x		Window x coord.
# y		Window y coord.

proc ::TreeCtrl::Leave1 {w x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	header {
	    if {[$w column cget $Priv(column) -state] eq "pressed"} {
		$w column configure $Priv(column) -state normal
	    }
	}
    }
    return
}

# ::TreeCtrl::Enter1 --
#
# Handle <Button1-Enter> event.
#
# Arguments:
# w		The treectrl widget.
# x		Window x coord.
# y		Window y coord.

proc ::TreeCtrl::Enter1 {w x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	default {}
    }
    return
}

# ::TreeCtrl::Release1 --
#
# Handle <ButtonRelease-1> event.
#
# Arguments:
# w		The treectrl widget.
# x		Window x coord.
# y		Window y coord.

proc ::TreeCtrl::Release1 {w x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	header {
	    if {[$w column cget $Priv(column) -state] eq "pressed"} {
		$w column configure $Priv(column) -state active
		TryEvent $w Header invoke [list C $Priv(column)]
	    }
	}
	dragColumn {
	    AutoScanCancel $w
	    $w column configure $Priv(column) -state normal
	    set visible [expr {[$w column dragcget -imagecolumn] ne ""}]
	    if {$visible} {
		$w column dragconfigure -imagecolumn ""
		set column [$w column dragcget -indicatorcolumn]
		if {($column ne "") && [$w column compare $column != $Priv(column)]} {
		    TryEvent $w ColumnDrag receive [list C $Priv(column) b $column]
		}
		$w column dragconfigure -indicatorcolumn ""
	    }
	    set id [$w identify $x $y]
	    if {[lindex $id 0] eq "header"} {
		set column [lindex $id 1]
		if {($column ne "") && [$w column compare $column != "tail"]} {
		    if {[$w column cget $column -button]} {
			$w column configure $column -state active
		    }
		}
	    }
	    TryEvent $w ColumnDrag end [list C $Priv(column)]
	}
	normal {
	    AutoScanCancel $w
	    set nearest [$w item id [list nearest $x $y]]
	    if {$nearest ne ""} {
		$w activate $nearest
	    }
set Priv(prev) ""
	}
	resize {
	    if {[$w cget -columnproxy] ne ""} {
		scan [$w column bbox $Priv(column)] "%d %d %d %d" x1 y1 x2 y2
		set width [expr {[$w cget -columnproxy] - $x1 + 1}]
		$w configure -columnproxy {}
		$w column configure $Priv(column) -width $width
		CursorCheck $w $x $y
	    }
	}
	rowlabel {
	    if {[$w cget -columnproxy] ne ""} {
		scan [$w contentbox] "%d %d %d %d" x1 y1 x2 y2
		incr x1 -[$w rowlabel width]
		set width [expr {[$w cget -columnproxy] - $x1 + 1}]
		$w configure -columnproxy {}
		$w configure -rowlabelwidth $width
		CursorCheck $w $x $y
	    }
	    if {[$w cget -rowproxy] ne ""} {
		scan [$w rowlabel bbox $Priv(rowlabel)] "%d %d %d %d" x1 y1 x2 y2
		set height [expr {[$w cget -rowproxy] - $y1 + 1}]
		$w configure -rowproxy {}
		$w rowlabel configure $Priv(rowlabel) -height $height
	    }
	}
    }
    unset Priv(buttonMode)
    return
}

# ::TreeCtrl::BeginSelect --
#
# This procedure is typically invoked on button-1 presses.  It begins
# the process of making a selection in the treectrl.  Its exact behavior
# depends on the selection mode currently in effect for the treectrl.
#
# Arguments:
# w		The treectrl widget.
# item		The item for the selection operation (typically the
#		one under the pointer).

proc ::TreeCtrl::BeginSelect {w item} {
    variable Priv
    if {$item eq ""} return
    if {[string equal [$w cget -selectmode] "multiple"]} {
	if {[$w selection includes $item]} {
	    $w selection clear $item
	} else {
	    $w selection add $item
	}
    } else {
	$w selection anchor $item
	$w selection modify $item all
	set Priv(selection) {}
	set Priv(prev) $item
    }
    return
}

# ::TreeCtrl::SelectionMotion --
#
# This procedure is called to process mouse motion events while
# button 1 is down.  It may move or extend the selection, depending
# on the treectrl's selection mode.
#
# Arguments:
# w		The treectrl widget.
# item-		The item under the pointer.

proc ::TreeCtrl::SelectionMotion {w item} {
    variable Priv

    if {$item eq ""} return
    if {$item eq $Priv(prev)} return
    if {![$w item enabled $item]} return

    switch [$w cget -selectmode] {
	browse {
	    $w selection modify $item all
	    set Priv(prev) $item
	}
	extended {
	    set i $Priv(prev)
	    set select {}
	    set deselect {}
	    if {$i eq ""} {
		set i $item
		lappend select $item
		set hack [$w item compare $item == anchor]
	    } else {
		set hack 0
	    }
	    if {[$w selection includes anchor] || $hack} {
		set deselect [concat $deselect [$w range $i $item]]
		set select [concat $select [$w range anchor $item]]
	    } else {
		set deselect [concat $deselect [$w range $i $item]]
		set deselect [concat $deselect [$w range anchor $item]]
	    }
	    if {![info exists Priv(selection)]} {
		set Priv(selection) [$w selection get]
	    }
	    while {[$w item compare $i < $item] && [$w item compare $i < anchor]} {
		if {[lsearch $Priv(selection) $i] >= 0} {
		    lappend select $i
		}
		set i [$w item id "$i next visible"]
	    }
	    while {[$w item compare $i > $item] && [$w item compare $i > anchor]} {
		if {[lsearch $Priv(selection) $i] >= 0} {
		    lappend select $i
		}
		set i [$w item id "$i prev visible"]
	    }
	    set Priv(prev) $item
	    $w selection modify $select $deselect
	}
    }
    return
}

# ::TreeCtrl::BeginExtend --
#
# This procedure is typically invoked on shift-button-1 presses.  It
# begins the process of extending a selection in the treectrl.  Its
# exact behavior depends on the selection mode currently in effect
# for the treectrl.
#
# Arguments:
# w		The treectrl widget.
# item-		The item for the selection operation (typically the
#		one under the pointer).

proc ::TreeCtrl::BeginExtend {w item} {
    if {[string equal [$w cget -selectmode] "extended"]} {
	if {[$w selection includes anchor]} {
	    SelectionMotion $w $item
	} else {
	    # No selection yet; simulate the begin-select operation.
	    BeginSelect $w $item
	}
    }
    return
}

# ::TreeCtrl::BeginToggle --
#
# This procedure is typically invoked on control-button-1 presses.  It
# begins the process of toggling a selection in the treectrl.  Its
# exact behavior depends on the selection mode currently in effect
# for the treectrl.
#
# Arguments:
# w		The treectrl widget.
# item		The item for the selection operation (typically the
#		one under the pointer).

proc ::TreeCtrl::BeginToggle {w item} {
    variable Priv
    if {$item eq ""} return
    if {[string equal [$w cget -selectmode] "extended"]} {
	set Priv(selection) [$w selection get]
	set Priv(prev) $item
	$w selection anchor $item
	if {[$w selection includes $item]} {
	    $w selection clear $item
	} else {
	    $w selection add $item
	}
    }
    return
}

# ::TreeCtrl::AutoScanCheck --
#
# Sees if the given pointer coords are outside the content area of the
# treectrl (ie, not including borders or column headers) or within
# -scrollmargin distance of the edges of the content area. If so and
# auto-scanning is not already in progress, then the window is scrolled
# and an [after] callback to AutoScanCheckAux is scheduled.
#
# Arguments:
# w		The treectrl widget.
# x		Window x coord.
# y		Window y coord.

proc ::TreeCtrl::AutoScanCheck {w x y} {
    variable Priv
    scan [$w contentbox] "%d %d %d %d" x1 y1 x2 y2
    set margin [winfo pixels $w [$w cget -scrollmargin]]
    if {($x < $x1 + $margin) || ($x >= $x2 - $margin) ||
	($y < $y1 + $margin) || ($y >= $y2 - $margin)} {
	if {![info exists Priv(autoscan,afterId,$w)]} {
	    if {$y >= $y2 - $margin} {
		$w yview scroll 1 units
		set delay [$w cget -yscrolldelay]
	    } elseif {$y < $y1 + $margin} {
		$w yview scroll -1 units
		set delay [$w cget -yscrolldelay]
	    } elseif {$x >= $x2 - $margin} {
		$w xview scroll 1 units
		set delay [$w cget -xscrolldelay]
	    } elseif {$x < $x1 + $margin} {
		$w xview scroll -1 units
		set delay [$w cget -xscrolldelay]
	    }
	    set count [scan $delay "%d %d" d1 d2]
	    if {[info exists Priv(autoscan,scanning,$w)]} {
		if {$count == 2} {
		    set delay $d2
		}
	    } else {
		if {$count == 2} {
		    set delay $d1
		}
		set Priv(autoscan,scanning,$w) 1
	    }
	    if {$Priv(autoscan,command,$w) ne ""} {
		set command [string map [list %T $w %x $x %y $y] $Priv(autoscan,command,$w)]
		eval $command
	    }
	    set Priv(autoscan,afterId,$w) [after $delay [list TreeCtrl::AutoScanCheckAux $w]]
	}
	return
    }
    AutoScanCancel $w
    return
}

# ::TreeCtrl::AutoScanCheckAux --
#
# Gets the location of the pointer and calls AutoScanCheck.
#
# Arguments:
# w		The treectrl widget.

proc ::TreeCtrl::AutoScanCheckAux {w} {
    variable Priv
    # Not quite sure how this can happen
    if {![info exists Priv(autoscan,afterId,$w)]} return
    unset Priv(autoscan,afterId,$w)
    set x [winfo pointerx $w]
    set y [winfo pointery $w]
    set x [expr {$x - [winfo rootx $w]}]
    set y [expr {$y - [winfo rooty $w]}]
    AutoScanCheck $w $x $y
    return
}

# ::TreeCtrl::AutoScanCancel --
#
# Cancels any pending [after] callback to AutoScanCheckAux.
#
# Arguments:
# w		The treectrl widget.

proc ::TreeCtrl::AutoScanCancel {w} {
    variable Priv
    if {[info exists Priv(autoscan,afterId,$w)]} {
	after cancel $Priv(autoscan,afterId,$w)
	unset Priv(autoscan,afterId,$w)
    }
    unset -nocomplain Priv(autoscan,scanning,$w)
    return
}

# ::TreeCtrl::ColumnDragScrollCheck --
#
# Sees if the given pointer coords are outside the left or right edges of
# the content area of the treectrl (ie, not including borders). If so and
# auto-scanning is not already in progress, then the window is scrolled
# horizontally and the column drag-image is repositioned, and an [after]
# callback to ColumnDragScrollCheckAux is scheduled.
#
# Arguments:
# w		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord or pointer.

proc ::TreeCtrl::ColumnDragScrollCheck {w x y} {
    variable Priv
    scan [$w contentbox] "%d %d %d %d" x1 y1 x2 y2
    if {($x < $x1) || ($x >= $x2)} {
	if {![info exists Priv(autoscan,afterId,$w)]} {
	    set bbox1 [$w column bbox $Priv(column)]
	    if {$x >= $x2} {
		$w xview scroll 1 units
	    } else {
		$w xview scroll -1 units
	    }
	    set bbox2 [$w column bbox $Priv(column)]
	    if {[lindex $bbox1 0] != [lindex $bbox2 0]} {
		incr Priv(columnDrag,x) [expr {[lindex $bbox2 0] - [lindex $bbox1 0]}]
		$w column dragconfigure -imageoffset [expr {$x - $Priv(columnDrag,x)}]
	    }
	    set Priv(autoscan,afterId,$w) [after 50 [list TreeCtrl::ColumnDragScrollCheckAux $w]]
	}
	return
    }
    AutoScanCancel $w
    return
}

# ::TreeCtrl::ColumnDragScrollCheckAux --
#
# Gets the location of the pointer and calls ColumnDragScrollCheck.
#
# Arguments:
# w		The treectrl widget.

proc ::TreeCtrl::ColumnDragScrollCheckAux {w} {
    variable Priv
    # Not quite sure how this can happen
    if {![info exists Priv(autoscan,afterId,$w)]} return
    unset Priv(autoscan,afterId,$w)
    set x [winfo pointerx $w]
    set y [winfo pointery $w]
    set x [expr {$x - [winfo rootx $w]}]
    set y [expr {$y - [winfo rooty $w]}]
    ColumnDragScrollCheck $w $x $y
    return
}

# ::TreeCtrl::UpDown --
#
# Returns the id of an item above or below the given item that the active
# item could be set to. If the given item isn't visible, the first visible
# enabled item is returned. An attempt is made to choose an item in the
# same column over repeat calls; this gives a better result if some rows
# have less items than others. Only enabled items are considered.
#
# Arguments:
# w		The treectrl widget.
# item		Item to move from, typically the active item.
# n		+1 to move down, -1 to move up.

proc ::TreeCtrl::UpDown {w item n} {
    variable Priv
    set rnc [$w item rnc $item]
    if {$rnc eq ""} {
	return [$w item id {first visible state enabled}]
    }
    scan $rnc "%d %d" row col
    set Priv(keyNav,row,$w) [expr {$row + $n}]
    if {![info exists Priv(keyNav,rnc,$w)] || $rnc ne $Priv(keyNav,rnc,$w)} {
	set Priv(keyNav,col,$w) $col
    }
    set item2 [$w item id "rnc $Priv(keyNav,row,$w) $Priv(keyNav,col,$w)"]
    if {[$w item compare $item == $item2]} {
	set Priv(keyNav,row,$w) $row
	if {![$w item enabled $item2]} {
	    return ""
	}
    } else {
	set Priv(keyNav,rnc,$w) [$w item rnc $item2]
	if {![$w item enabled $item2]} {
	    return [UpDown $w $item2 $n]
	}
    }
    return $item2
}

# ::TreeCtrl::LeftRight --
#
# Returns the id of an item left or right of the given item that the active
# item could be set to. If the given item isn't visible, the first visible
# enabled item is returned. An attempt is made to choose an item in the
# same row over repeat calls; this gives a better result if some columns
# have less items than others. Only enabled items are considered.
#
# Arguments:
# w		The treectrl widget.
# item		Item to move from, typically the active item.
# n		+1 to move right, -1 to move left.

proc ::TreeCtrl::LeftRight {w item n} {
    variable Priv
    set rnc [$w item rnc $item]
    if {$rnc eq ""} {
	return [$w item id {first visible state enabled}]
    }
    scan $rnc "%d %d" row col
    set Priv(keyNav,col,$w) [expr {$col + $n}]
    if {![info exists Priv(keyNav,rnc,$w)] || $rnc ne $Priv(keyNav,rnc,$w)} {
	set Priv(keyNav,row,$w) $row
    }
    set item2 [$w item id "rnc $Priv(keyNav,row,$w) $Priv(keyNav,col,$w)"]
    if {[$w item compare $item == $item2]} {
	set Priv(keyNav,col,$w) $col
	if {![$w item enabled $item2]} {
	    return ""
	}
    } else {
	set Priv(keyNav,rnc,$w) [$w item rnc $item2]
	if {![$w item enabled $item2]} {
	    return [LeftRight $w $item2 $n]
	}
    }
    return $item2
}

# ::TreeCtrl::SetActiveItem --
#
# Sets the active item, scrolls it into view, and makes it the only selected
# item. If -selectmode is extended, makes the active item the anchor of any
# future extended selection.
#
# Arguments:
# w		The treectrl widget.
# item		The new active item, or "".

proc ::TreeCtrl::SetActiveItem {w item} {
    if {$item eq ""} return
    $w activate $item
    $w see active
    $w selection modify active all
    switch [$w cget -selectmode] {
	extended {
	    $w selection anchor active
	    set Priv(prev) [$w item id active]
	    set Priv(selection) {}
	}
    }
    return
}

# ::TreeCtrl::Extend --
#
# Does nothing unless we're in extended selection mode;  in this
# case it moves the location cursor (active item) up, down, left or
# right, and extends the selection to that point.
#
# Arguments:
# w		The treectrl widget.
# dir		up, down, left or right

proc ::TreeCtrl::Extend {w dir} {
    variable Priv
    if {[string compare [$w cget -selectmode] "extended"]} {
	return
    }
    if {![info exists Priv(selection)]} {
	$w selection add active
	set Priv(selection) [$w selection get]
    }
    switch -- $dir {
	above { set item [UpDown $w active -1] }
	below { set item [UpDown $w active 1] }
	left { set item [LeftRight $w active -1] }
	right { set item [LeftRight $w active 1] }
    }
    if {$item eq ""} return
    $w activate $item
    $w see active
    SelectionMotion $w [$w item id active]
    return
}

# ::TreeCtrl::DataExtend
#
# This procedure is called for key-presses such as Shift-KEndData.
# If the selection mode isn't multiple or extended then it does nothing.
# Otherwise it moves the active item and, if we're in
# extended mode, extends the selection to that point.
#
# Arguments:
# w		The treectrl widget.
# item		Item to become new active item.

proc ::TreeCtrl::DataExtend {w item} {
    if {$item eq ""} return
    set mode [$w cget -selectmode]
    if {[string equal $mode "extended"]} {
	$w activate $item
	$w see $item
        if {[$w selection includes anchor]} {
	    SelectionMotion $w $item
	}
    } elseif {[string equal $mode "multiple"]} {
	$w activate $item
	$w see $item
    }
    return
}

# ::TreeCtrl::Cancel
#
# This procedure is invoked to cancel an extended selection in
# progress.  If there is an extended selection in progress, it
# restores all of the items between the active one and the anchor
# to their previous selection state.
#
# Arguments:
# w		The treectrl widget.

proc ::TreeCtrl::Cancel w {
    variable Priv
    if {[string compare [$w cget -selectmode] "extended"]} {
	return
    }
    set first [$w item id anchor]
    set last $Priv(prev)
    if { [string equal $last ""] } {
	# Not actually doing any selection right now
	return
    }
    if {[$w item compare $first > $last]} {
	set tmp $first
	set first $last
	set last $tmp
    }
    $w selection clear $first $last
    while {[$w item compare $first <= $last]} {
	if {[lsearch $Priv(selection) $first] >= 0} {
	    $w selection add $first
	}
	set first [$w item id "$first next visible"]
    }
    return
}

# ::TreeCtrl::SelectAll
#
# This procedure is invoked to handle the "select all" operation.
# For single and browse mode, it just selects the active item.
# Otherwise it selects everything in the widget.
#
# Arguments:
# w		The treectrl widget.

proc ::TreeCtrl::SelectAll w {
    set mode [$w cget -selectmode]
    if {[string equal $mode "single"] || [string equal $mode "browse"]} {
	$w selection modify active all
    } else {
	$w selection add all
    }
    return
}

# ::TreeCtrl::MarqueeBegin --
#
# Shows the selection rectangle at the given coords.
#
# Arguments:
# w		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord or pointer.

proc ::TreeCtrl::MarqueeBegin {w x y} {
    set x [$w canvasx $x]
    set y [$w canvasy $y]
    $w marquee coords $x $y $x $y
    $w marquee configure -visible yes
    return
}

# ::TreeCtrl::MarqueeUpdate --
#
# Resizes the selection rectangle.
#
# Arguments:
# w		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord or pointer.

proc ::TreeCtrl::MarqueeUpdate {w x y} {
    set x [$w canvasx $x]
    set y [$w canvasy $y]
    $w marquee corner $x $y
    return
}

# ::TreeCtrl::MarqueeEnd --
#
# Hides the selection rectangle.
#
# Arguments:
# w		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord or pointer.

proc ::TreeCtrl::MarqueeEnd {w x y} {
    $w marquee configure -visible no
    return
}

# ::TreeCtrl::ScanMark --
#
# Marks the start of a possible scan drag operation.
#
# Arguments:
# w		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord or pointer.

proc ::TreeCtrl::ScanMark {w x y} {
    variable Priv
    $w scan mark $x $y
    set Priv(x) $x
    set Priv(y) $y
    set Priv(mouseMoved) 0
    return
}

# ::TreeCtrl::ScanDrag --
#
# Performs a scan drag if the mouse moved.
#
# Arguments:
# w		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord or pointer.

proc ::TreeCtrl::ScanDrag {w x y} {
    variable Priv
    if {![info exists Priv(x)]} { set Priv(x) $x }
    if {![info exists Priv(y)]} { set Priv(y) $y }
    if {($x != $Priv(x)) || ($y != $Priv(y))} {
	set Priv(mouseMoved) 1
    }
    if {[info exists Priv(mouseMoved)] && $Priv(mouseMoved)} {
	$w scan dragto $x $y
    }
    return
}

# ::TreeCtrl::TryEvent --
#
# This procedure is used to cause a treectrl to generate a dynamic event.
# If the treectrl doesn't have the event defined (because you didn't call
# the [notify install] command) nothing happens. TreeCtrl::PercentsCmd is
# used to perform %-substitution on any scripts bound to the event.
#
# Arguments:
# T		The treectrl widget.
# event		Name of event.
# detail	Name of detail or "".
# charMap	%-char substitution list (even number of elements).

proc ::TreeCtrl::TryEvent {T event detail charMap} {
    if {[lsearch -exact [$T notify eventnames] $event] == -1} return
    if {$detail ne ""} {
	if {[lsearch -exact [$T notify detailnames $event] $detail] == -1} return
	$T notify generate <$event-$detail> $charMap "::TreeCtrl::PercentsCmd $T"
    } else {
	$T notify generate <$event> $charMap "::TreeCtrl::PercentsCmd $T"
    }
    return
}

# ::TreeCtrl::PercentsCmd --
#
# This command is passed to [notify generate] to perform %-substitution on
# scripts bound to dynamic events. It supports the same set of substitution
# characters as the built-in static events (plus any event-specific chars).
#
# Arguments:
# T		The treectrl widget.
# char		%-char to be replaced in bound scripts.
# object	Same arg passed to [notify bind].
# event		Name of event.
# detail	Name of detail or "".
# charMap	%-char substitution list (even number of elements).

proc ::TreeCtrl::PercentsCmd {T char object event detail charMap} {
    if {$detail ne ""} {
	set pattern <$event-$detail>
    } else {
	set pattern <$event>
    }
    switch -- $char {
	d { return $detail }
	e { return $event }
	P { return $pattern }
	W { return $object }
	T { return $T }
	? {
	    array set map $charMap
	    array set map [list T $T W $object P $pattern e $event d $detail]
	    return [array get map]
	}
	default {
	    array set map [list $char $char]
	    array set map $charMap
	    return $map($char)
	}
    }
    return
}
