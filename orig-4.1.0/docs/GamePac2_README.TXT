Last updated: June 1, 2006 by Everett Kaser
All files in this ZIP were saved in LIFUTIL LFS format.

The HP-85 Games Pac II was created because a number of folks at the
Corvallis, Oregon HP division wrote a bunch of games in the first couple
of years after the release of the HP-85, so why let all that hard work
go to waste?  Thus was born the HP-85 Games Pac II.

At this point in time (June 2006), I'm not aware of any existing
ORIGINAL copy of this package, so there is no manual (and thus few
instructions).  But I recently tracked down a copy of the games that was
in the possesion of Clive Small in Australia.  In 1990, he'd been in
contact with Greg Goebel who was still in marketing at HP (Greg had been
in a marketing position at Corvallis in the early- to mid-1980's also,
which was when I knew him).  Since the HP-85 was out of support life at
that point, Greg sent Clive a copy of all of the games he had on hand at
that time, and Clive had copied them to his hard disk along with the
original Games Pac.  Clive also had some other HP-85 equipment, including
an original HP-85 Assembler ROM package, which included one issue of the
HP BasicExchange newsletter.  By lucky coincidence, that issue had a one
and a half page article on the Games II pack.

While putting together this "re-creation" of the HP-85 Games Pac II, I've
also assembled an "HP-85 Games Pac Misc" which contains miscellaneous
games and alternate versions of games that weren't on the Games Pacs I and II.
I'll be including a copy of this README.TXT file in both packages, since they
contain some related material.

The games that were part of the HP-85 Games Pac II are:
FATCAT (and FATB)   A PacMan type game, clear up the cheese, avoid the cat
DODGE (and DODBIN)  Dodge the blocks on the scrolling screen, clear the ++'s
RATPAK (and RATB)   Drive around city blocks shooting rats before you get bit
SPACE				The HP version of the then common Star Trek game.
BATS (and BATB)	    Shoot the swooping bats, dodge their dung
GNOME (and GNDATA)  An early text adventure game (this game was also in the
TANK   (and TNKBIN) Fire from a rotating turret to shoot targets and defend
ASTER (and ASTBIN)	Shoot a supply capsule through asteroids to a space station
BULPEN (and BULLB)  Corral the bulls by pushing blocks
MOONL				Graphical moon-landing game.
BARRAG (and BARB)   A Space Invaders clone
GOLFER (and COURSE) Playing golf at Holly Pines Golf Course

Games in the Games Pac Misc collection are:
STREK               This is an earlier version of the SPACE game, in its original
                    "Star Trek" form.  There were many "Star Trek" games like 
                    this on various computers at the time.  However, HP couldn't
                    release a "Star Trek" game without doing licensing, so it was
                    renamed LUBACH.  Why "Lubach"?  The bad guys were from Lubach 
                    instead of Klingon, because Texas Instruments was HP's primary
					competitor at the time in the calculator market (which
					was mostly what was made at Corvallis division), and TI's
					calculators were made in... are you ready?  Lubach, Texas.
					The "Scotty" character was renamed "Willy" in 'honor' of
					Bill (William) Kemper, and Spock was renamed "Kenewk" in 
					'honor' of Ken Newcomer, two of the Applications Engineers
					for the calculators and the HP-85.
TURRET (and TURBIN) An earlier version of TANK, written mostly in BASIC with a 
					binary program that just provided the ability to output text
					on the alpha screen at a row/column location.  TANK, however,
					was written almost entirely as a binary program, with only a
					shell written in BASIC.
CUBIC               A 4x4x4 tic-tac-toe
FTBALL              American football
CANNON              Shoot a canon over a wall and hit a cow
CRAPS				dice game
YAHTZEE				another dice game :-)
SKI					from the HP-85 Standard Pac

Some of the BASIC and/or binary programs have dates in them, which give some
indication as to when they were completed.  However, I know that some of these
dates were added later on (in the case of my own early games, at least).  CANNON
is the only game that actually has the author's name in it, but I know who
wrote many of them.  The dates are "completion dates", as recorded in the programs
themselves.

name	date		 authors
------  ------------ -------------------------------------------------------------------
GNOME   Feb  5, 1980 multiple (starting with Nelson Mills), based upon HP-3000 Adventure
TURRET  Mar 17, 1980 Everett Kaser
CANNON  Apr  5, 1980 Bill Frolik
DODGE   Apr 13, 1980 Everett Kaser
ASTER	Jun  5, 1980 Everett Kaser
BULPEN  Oct  5, 1980 Everett Kaser
BARRAG  Nov  3, 1980 Randy Salo
RATPAK  Jan  4, 1980 Everett Kaser (this date is NOT be right! Should be Jan 1981)
FATCAT  Mar 16, 1981 Randy Salo
BATS    Apr 23, 1981 Everett Kaser
TANK    May 26, 1981 Everett Kaser (rewrite of TURRET)
FTBALL  unknown      unknown
CUBIC   unkonwn      unknown
GOLFER  unknown      unknown
STREK   unknown      unknown
SPACE	unknown		 unknown
MOONL	unknown		 unknown
CRAPS   unknown		 unknown
YAHTZEE unknown		 unknown (ported by John Shadbolt)
SKI		1979		 unknown (from HP-85 Standard Pac)

I'm sure that the date in RATPAK was originally Nov/Dec of 1980, and when
it was finished in Jan 1981, I edited the month/day, but forgot to update
the year.  I KNOW that I didn't write RATPAK in Jan 1980, as I hadn't even
written my first HP-85 game (TURRET) at that point.  Further, Jan 1981 puts
it, chronologically, in just about the right place in the above list according
to my memory of which games showed up in which order.

The GOLFER "course" hole layouts were from drawings Mike Dunn had of the
Trysting Tree Golf Course, a golf course backed by Oregon State University
that was in the planning stages at the time (and has since been created
on the east bank of the Willamette River).  Who actually wrote the GOLFER
game, I don't know.

Many of these games require the Mass Storage ROM in order to run.  STREK requires
the Plotter/Printer ROM (because it was tokenized with that ROM present).

==========================
MISCELLANEOUS INSTRUCTIONS
==========================

Some of the games include proper key labelling on the screen to show you
what to press to cause which action, however some don't.  Not having a
manual to include, I started to document the games, but have only gotten
to FATCAT so far.  Hopefully, between the BasicExchange article and the
text on the screen, you can figure them out.  I'll add more comments
about the other games as time allows.

FATCAT
------
There are 9 levels of difficulty (speeds of the cat).  At MEDIUM, the mouse
and cat move at the same speed.  K1 and K2 can be used to make it harder
(faster cat) or easier (slower cat).  K3 turns the sound off and on, and
K4 starts the game (enters the binary program).

NOTE: if you examine the BASIC program code, you will find the call to the
MOUSE() function.  The last argument is always 0.  This is the starting LEVEL#.
If you change that value to anywhere from 0 to 34 (there are 35 levels total),
you can control the starting level number.

Once in the game itself:
  K1 turns the mouse LEFT (from the MOUSE's perspective, NOT yours!)
  K2 and K3 make the mouse "jump" to a random location.  You can
     do this only ONCE per mouse.
  K4 turns the mouse RIGHT (from the MOUSE's perspective, NOT yours!)
  
You must eat ALL of the cheese (erase all of the dots) without letting the
cat catch you.  There are "mouse holes" (gaps in the vertical walls) on
the left and right that let the mouse wrap around from left-to-right, but
which the cat can't go through.  Using these, you can get away from the
cat, put a little ground between you.

NOTE: the mouse can only turn left/right when in the center of a 'cell'
(a 'square' of the maze).  Once the mouse starts to move from one cell
to another, it will continue to move until it arrives at that cell.  As
soon as it arrives there, it checks to see if a key has been pressed to
turn it, and takes the appropriate action if so.  Only ONE key press
will be accepted per cell.  You can use this knowledge to better control
the mouse, hitting the turn keys BEFORE reaching the corner (but not TOO
far before...).

As soon as the last piece of cheese is eaten the level ends, and play
proceeds to the next level.  To start the next level, you can press a
turn key (K1 or K4) or press the SPACEBAR if you wish to move straight
ahead.

You start with three mice (one in the maze and two "in reserve" in the
scoreboard line).  You are awarded one extra mouse (life) for each level
you complete.  When you run out of mice, the game is over.

BULPEN
------
Briefly: you move your player as in FATCAT (above), turning left/right
from the PLAYER'S point of view, not YOUR point of view.  To win, you
have to enclose all of the bulls with blocks AND squeeze that enclosure
down until the bulls can't move at all.  It's harder than it looks! :-)
