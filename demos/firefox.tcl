proc DemoFirefoxPrivacy {} {

	set T .f2.f1.t

	#
	# Configure the treectrl widget
	#

	$T configure -showroot no -showbuttons yes -showlines no \
		-selectmode extended -xscrollincrement 20 -showheader yes

	# Hide the borders because child windows appear on top of them
	$T configure -borderwidth 0 -highlightthickness 0

	#
	# Create columns
	#

	# Create 2 new images for the buttona arrow
	if {[lsearch -exact [image names] arrow-up] == -1} {

		set color #ACA899 ; # WinXP arrow color

		set img arrow-down
		image create photo $img
		$img put [list [string repeat "$color " 9]] -to 0 0
		$img put [list [string repeat "$color " 7]] -to 1 1
		$img put [list [string repeat "$color " 5]] -to 2 2
		$img put [list [string repeat "$color " 3]] -to 3 3
		$img put [list [string repeat "$color " 1]] -to 4 4

		set img arrow-up
		image create photo $img
		$img put [list [string repeat "$color " 1]] -to 4 0
		$img put [list [string repeat "$color " 3]] -to 3 1
		$img put [list [string repeat "$color " 5]] -to 2 2
		$img put [list [string repeat "$color " 7]] -to 1 3
		$img put [list [string repeat "$color " 9]] -to 0 4
	}

	$T column create -expand yes -arrowimage {arrow-down !up arrow-up {}} \
		-arrow up -arrowpadx {10 2} -textlines 0 \
		-text "This is a multi-line column title\nwith an image for the arrow"

	# This binding toggles the sort arrow
	$T notify bind $T <Header-invoke> {
		if {[%T column cget %C -arrow] eq "up"} {
			%T column configure %C -arrow down
		} else {
			%T column configure %C -arrow up
		}
	}

	#
	# Create elements
	#

	$T element create eWindow window
	$T element create eText1 text -font [list "[$T cget -font] bold"]
	$T element create eText2 text

	#
	# Create styles using the elements
	#

	set S [$T style create styCategory -orient horizontal]
	$T style elements $S {eText1 eWindow}
	$T style layout $S eText1 -expand ns
	$T style layout $S eWindow -expand nws -padx 10 -pady 6

	set S [$T style create styFrame -orient horizontal]
	$T style elements $S eWindow
	$T style layout $S eWindow -iexpand e -squeeze x

	#
	# Create items and assign styles
	#

	foreach category {
		"History"
		"Saved Form Information"
		"Saved Passwords"
		"Download Manager History"
		"Cookies"
		"Cache"} {
		set I [$T item create -button yes]
		$T item style set $I 0 styCategory
		$T item element configure $I 0 eText1 -text $category
		set b [button $T.b$I -text "Clear" -command "" -width 11]
		$T item element configure $I 0 eWindow -window $b
		$T item lastchild root $I
	}

	set bg [$T cget -background]
	set textBg $bg

	# History
	set I [$T item create]
	$T item style set $I 0 styFrame
	set f [frame $T.f$I -borderwidth 0 -background $bg]
	label $f.l1 -background $bg -text "Remember visited pages for the last"
	entry $f.e1 -width 6 -background $bg
	$f.e1 insert end 20
	label $f.l2 -background $bg -text "days" -background $bg
	pack $f.l1 -side left
	pack $f.e1 -side left -padx 8
	pack $f.l2 -side left
	$T item element configure $I 0 eWindow -window $f
	$T item lastchild [$T index "root child 0"] $I

	# Saved Form Information
	set I [$T item create]
	$T item style set $I 0 styFrame
	set f [frame $T.f$I -borderwidth 0 -background $bg]
	text $f.t1 -background $textBg -borderwidth 0 -width 10 -height 1 -wrap word -cursor ""
	$f.t1 insert end "Information entered in web page forms and the Search Bar is saved to make filling out forms and searching faster."
	bindtags $f.t1 TextWrapBindTag
	checkbutton $f.cb1 -background $bg -text "Save information I enter in web page forms and the Search Bar"
	$f.cb1 select
	pack $f.t1 -side top -anchor w -fill x -padx {0 10} -pady {0 4}
	pack $f.cb1 -side top -anchor w
	$T item element configure $I 0 eWindow -window $f
	$T item lastchild [$T index "root child 1"] $I

	# Saved Passwords
	set I [$T item create]
	$T item style set $I 0 styFrame
	set f [frame $T.f$I -borderwidth 0 -background $bg]

	set fLeft [frame $f.fLeft -borderwidth 0 -background $bg]
	text $fLeft.t1 -background $textBg -borderwidth 0 -width 10 -height 1 -wrap word -cursor ""
	$fLeft.t1 insert end "Login information for web pages can be kept in the Password Manager so that you do not need to re-enter your login details every time you visit."
	bindtags $fLeft.t1 TextWrapBindTag
	checkbutton $fLeft.cb1 -background $bg -text "Remember Passwords"
	$fLeft.cb1 select
	pack $fLeft.t1 -side top -expand yes -fill x -pady {0 4}
	pack $fLeft.cb1 -side top -anchor w

	set fRight [frame $f.fRight -borderwidth 0 -background $bg]
	button $fRight.b1 -text "View Saved Passwords"
	button $fRight.b2 -text "Change Master Password..."
	pack $fRight.b1 -side top -expand yes -fill x
	pack $fRight.b2 -side top -expand yes -fill x -pady {8 0}
	pack $fLeft -side left -expand yes -fill x
	pack $fRight -side right -padx 14 -anchor n
	$T item element configure $I 0 eWindow -window $f
	$T item lastchild [$T index "root child 2"] $I

	# Download Manager History
	set I [$T item create]
	$T item style set $I 0 styFrame
	set f [frame $T.f$I -borderwidth 0 -background $bg]
	text $f.t1 -background $textBg -borderwidth 0 -width 10 -height 1 -wrap word -cursor ""
	$f.t1 insert end "The Download Manager keeps track of recently downloaded files."
	bindtags $f.t1 TextWrapBindTag

	set f1 [frame $f.f1 -borderwidth 0 -background $bg]
	label $f1.l1 -background $bg -text "Remove files from the Download Manager:"
	menubutton $f1.mb1 -indicatoron yes -menu $f1.mb1.m -text Manually \
		-width [string length "Upon successful download"] -justify left
	set m [menu $f1.mb1.m -tearoff no]
	foreach label {
		"Upon successful download"
		"When firefox exits"
		Manually} {
		$m add command -label $label -command [list $f1.mb1 configure -text $label]
	}
	pack $f1.l1 -side left
	pack $f1.mb1 -side left -padx {8 10}
	pack $f.t1 -side top -expand yes -fill x -padx {0 10}
	pack $f1 -side top -anchor w
	$T item element configure $I 0 eWindow -window $f
	$T item lastchild [$T index "root child 3"] $I

	# Cookies
	set I [$T item create]
	$T item style set $I 0 styFrame
	set f [frame $T.f$I -borderwidth 0 -background $bg]
	text $f.t1 -background $textBg -borderwidth 0 -width 10 -height 1 -wrap word -cursor ""
	$f.t1 insert end "Cookies are pieces of information stored by web pages on your computer. They are used to remember login information and other data."
	bindtags $f.t1 TextWrapBindTag

	set fLeft [frame $f.fLeft -borderwidth 0 -background $bg]
	checkbutton $fLeft.cb1 -background $bg -text "Allow sites to set cookies"
	$fLeft.cb1 select
	checkbutton $fLeft.cb2 -background $bg -text "for the originating web site only"
	pack $fLeft.cb1 -side top -anchor w
	pack $fLeft.cb2 -side top -anchor w -padx {20 0}

	set fRight [frame $f.fRight -borderwidth 0 -background $bg]
	button $fRight.b1 -text "Exceptions"
	button $fRight.b2 -text "View Cookies"
	pack $fRight.b1 -side left -padx {0 10}
	pack $fRight.b2 -side left

	set f1 [frame $fLeft.f1 -borderwidth 0 -background $bg]
	label $f1.l1 -background $bg -text "Keep Cookies:"
	menubutton $f1.mb1 -indicatoron yes -menu $f1.mb1.m -text "until they expire" \
		-width [string length "until I close Firefox"] -justify left
	set m [menu $f1.mb1.m -tearoff no]
	foreach label {
		"until they expire"
		"until I close Firefox"
		"ask me every time"} {
		$m add command -label $label -command [list $f1.mb1 configure -text $label]
	}
	pack $f1.l1 -side left
	pack $f1.mb1 -side left -padx {8 0}
	pack $f1 -side top -anchor w

	pack $f.t1 -side top -expand yes -fill x -padx {0 10} -pady {0 8}
	pack $fLeft -side left -expand yes -fill x
	pack $fRight -side right -padx 14 -anchor n
	$T item element configure $I 0 eWindow -window $f
	$T item lastchild [$T index "root child 4"] $I

	# Cache
	set I [$T item create]
	$T item style set $I 0 styFrame
	set f [frame $T.f$I -borderwidth 0 -background $bg]
	text $f.t1 -background $textBg -borderwidth 0 -width 10 -height 1 -wrap word -cursor ""
	$f.t1 insert end "Pages you view are stored in the cache for quicker viewing later on."
	bindtags $f.t1 TextWrapBindTag
	set f1 [frame $f.f1 -borderwidth 0 -background $bg]
	label $f1.l1 -background $bg -text "Use up to:"
	entry $f1.e1 -width 10 -background $bg
	$f1.e1 insert end 50000
	label $f1.l2 -background $bg -text "KB of disk space for the cache." -background $bg
	pack $f1.l1 -side left
	pack $f1.e1 -side left -padx 8
	pack $f1.l2 -side left
	pack $f.t1 -side top -expand yes -fill x -padx {0 10}
	pack $f1 -side top -anchor w
	$T item element configure $I 0 eWindow -window $f
	$T item lastchild [$T index "root child 5"] $I

	# This binding configures the -height option of a Text widget to the
	# number of lines it is displaying
	bind TextWrapBindTag <Configure> {
		scan [textlayout [%W cget -font] [%W get 1.0 "end - 1 chars"] -width %w] "%%d %%d" width height
		%W configure -height [expr {$height / [font metrics [%W cget -font] -linespace]}]
	}

	# This binding collapses all items before expanding a new one
	$T notify bind $T <Expand-before> {
		%T collapse all
	}
	$T item collapse all

	return
}
