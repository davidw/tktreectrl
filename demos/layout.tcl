#
# Demo: Layout
#
proc DemoLayout {} {

	set T .f2.f1.t

	$T configure -showroot no -showrootbutton yes -showbuttons yes \
		-showlines yes -itemheight 0 -selectmode browse

	$T column configure 0 -text Layout

	$T element create e1 rect -width 30 -height 30 -fill gray20
	$T element create e2 rect -width 30 -height 30 -fill gray40 \
		-outline blue -outlinewidth 3
	$T element create e3 rect -fill gray60
	$T element create e4 rect -fill [list $::SystemHighlight {selected focus} gray80 {}] \
		-showfocus yes
	$T element create e5 rect -fill {"sky blue"} -width 20 -height 20
	$T element create e6 rect -fill {"sea green"} -width 30 -height 16
	$T element create e7 rect -fill {"sky blue"} -width 30 -height 16
	$T element create e8 rect -fill gray70 -height 1

	set S [$T style create s1]
	$T style elements $S {e4 e3 e1 e2 e5 e6 e7}
	$T style layout $S e1 -padw 28 -pade 4 -padn 4 -pads 4
	$T style layout $S e2 -expand es -pade 38
	$T style layout $S e3 -union [list e1 e2] -ipadw 4 -ipade 4 -ipadn 4 -ipads 4 -padn 2 -pads 2
	$T style layout $S e4 -detach yes -iexpand es
	$T style layout $S e5 -detach yes -padw 2 -padn 2 -pads 2 -iexpand s
	$T style layout $S e6 -detach yes -expand ws -pade 2 -padn 2
	$T style layout $S e7 -detach yes -expand wn -pade 2 -pads 2

	set I [$T item create]
	$T item hasbutton $I yes
	$T item style set $I 0 $S
	$T item lastchild root $I
	set parent $I

	set I [$T item create]
	$T item hasbutton $I no
	$T item style set $I 0 $S
	$T item lastchild $parent $I

	###

	set S [$T style create s2]
	$T style elements $S {e4 e3 e1}
	$T style layout $S e1 -padw 8 -pade 8 -padn 8 -pads 8 -iexpand e
	$T style layout $S e3 -union e1 -ipadw 20 -ipade 4 -ipadn 4 -ipads 12
	$T style layout $S e4 -detach yes -iexpand es

	set I [$T item create]
	$T item hasbutton $I yes
	$T item style set $I 0 $S
	$T item lastchild root $I

	set I2 [$T item create]
	$T item hasbutton $I2 no
	$T item style set $I2 0 $S
	$T item lastchild $I $I2

	###

	set S [$T style create s3]
	$T style elements $S {e4 e3 e1 e5 e6}
	$T style layout $S e4 -union {e1 e6} -ipadw 8 -ipadn 8 -ipade 8
	$T style layout $S e3 -union {e1 e5} -ipadw 4 -ipadn 4 -ipade 4 -ipads 4
	$T style layout $S e5 -ipads 20

	set I [$T item create]
	$T item hasbutton $I yes
	$T item style set $I 0 $S
	$T item lastchild root $I

	set I2 [$T item create]
	$T item hasbutton $I2 no
	$T item style set $I2 0 $S
	$T item lastchild $I $I2

	###

	$T element create eb border -background $::SystemButtonFace \
		-relief {sunken {selected} raised {}} -thickness 2 -filled yes
	$T element create et text

	set text "Here is a text element surrounded by a border element.\nResize the column to watch me wrap."

	set S [$T style create s4]
	$T style elements $S {eb et}
	$T style layout $S eb -union et -ipadw 2 -ipadn 2 -ipade 2 -ipads 2
	$T style layout $S et -squeeze x

	set I [$T item create]
	$T item hasbutton $I yes
	$T item style set $I 0 $S
	$T item text $I 0 $text
	$T item lastchild root $I
	set parent $I

	set I [$T item create]
	$T item hasbutton $I no
	$T item style set $I 0 $S
	$T item text $I 0 $text
	$T item lastchild $parent $I

	###

	set styleNum 5
	foreach {orient expandList} {horizontal {s ns n} vertical {e we w}} {
		foreach expand $expandList {

			set S [$T style create s$styleNum -orient $orient]
			$T style elements $S {e4 e8 e2 e5 e6}
			$T style layout $S e4 -detach yes -iexpand es
			$T style layout $S e8 -detach yes -expand n -iexpand e
			$T style layout $S e2 -expand $expand
			$T style layout $S e5 -expand $expand
			$T style layout $S e6 -expand $expand
			incr styleNum

			set I [$T item create]
			$T item style set $I 0 $S
			$T item lastchild root $I
		}
	}

	return
}

