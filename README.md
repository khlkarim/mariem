mariem - A Visual Modeling Language
<video>

Building the executable:

Dependencies: 
I opted, whenever i could, for stb style libraries because they greatly simplify the build process.
These are the libraries that i use, you shouldn't worry about these since they are included in the repo and are built with the main executable:
- glad: for opengl function loading **
- nob.h: used as a build system and for logging
- miniaudio.h: i used its high level API for audio playback
- tinyfiledialogs: for opening native message and file dialog

BUT the library that you should worry about is glfw, since i couldn't find any good minimal alternatives. (there is RFGW, but its support for wayland is experimental, and guess what windowing system i use!)
So for now you have to build GLFW from source and edit the nob.c file at the root of the project to link with 
The official guide for compiling GLFW is pretty informative and should do the job.
Atfer compiling it, you will end up with a libglfw3.a file 

on Linux 
cc -o nob nob.c
./nob 

this will create a build directory if it does not exist and it will also 
compile the project to  ./build/main 

to compile the random project generator ./nob gen 
to create a debug build: (it will produce gmont.a) files ./nob main debug 

Usage: 
Running the compiled executable will put you face to face with the editor.
The interaction with it keyboard centric, these are the main keybindings that let you do stuff:
- create and delete nodes
- add and remove colors
- bind files to colors
- color nodes
- create links between nodes and color nodes 

After you finished creating your nodes, linking them and giving them pretty colors, you can save your project to a file!
loading a project 
to make sure that everything is working for you can pick a file from the examples directory and load it as a project 
pressing the space bar should make something happen  

interacting with the app
keybindings
since there is not that many things to do 
the interaction is keyboard centric here are all the keybindings
they are defined as macros at the start of the ./src/main.c file, you can just change them there and recompile the app if you want to customize them 

Windows land
i have been trying for two days to cross compile to windows using mingw and wine
i have got to compile using this command:

gcc -o ./build/main.exe ./src/main.c ./src/glad.c ./src/tinyfiledialogs.c -I./include -static ./lib/build/src/libglfw3.a -lgdi32 -lcomdlg32 -lole32 

Motivation
What is mariem? And why do i call it a language?
Well to answer that, i think we first need to answer a deeper question first:
Why did i even make it in the first place?

Have you ever tried to use FlStudio?
its a tool, more specifically a DAW - Digital audio interface, That lets you
And if you have ever used it, you are probably familiar with this interface:

oh yea, its so intimiditing  

i did not need all that
i had a simple mental model of beat making and 
lived mostly in the beat maker
but it kept bugging 

this irritation pushed me to implemente the simplest way i can think of making music
this irritation pushed me to implemente my own mental model of music: different sounds that get played at discreate intervals of time 

this problem kept ---- at the back of my head 
now i iterated on this problem a couple of time 
even tried making a simple API for making beats in python 
its slow, very OOP and 


NOW THE ANSWER TO THE QUESTION! god i be yapping

last semester i took a systems modeling course, it serves as an introduction to topics like: Petri nets, CPT and the b-method
and there and then was mariem born, its the god daugther of colored petri nets and the b-method 
if you take a colored petri nets graph and consider each transitions as an operation having a precondition and a postcondition
and with some component-X, you add some determinism to the transitions, and kaboom you get mariem!
were it takes the colored directed graph concept from CPT and the assertions and the performs from the B-method

The mariem Manifest - My attempt at a formal definition:
mariem is a modeling language, a kin to petri nets or state machines.

mariem defines 5 primitive concepts 

The state of a program is a directed colored graph.
The special thing about this graph is that only the node have colors but also the links between two!

On top of this state is defined a meta language that has two types of statements, each link can have optionally have each of these types of statements attached to it:
- assert statements: an assert statement is a pre-condition on the coloring of the graph that defines when the link/transition is triggerable 
- perform statements: a perform statement is a post-condition/coloring-change that gets performed after its corresponding link gets executed

Here is an example of an assert statement:
<gif> 

Here is an example of a perform statement:
<gif>

And that's it!
I don't know if this has cleared anything for you yet, but i think a tutorial would do the job!

Tutorial: Lets make some musik!

The purpose of this section is to get you familiar with the workflow of using the editor and how to translate your ideas into colorful graphs!
We are going to make the Runaway melody seen in the demo video. 

Here is the chords view from a remake of the song in FlStudio:
<pic>

How would we represent this as graph?
Well the naive approach would be to make a node for every beat, and color the nodes with the color of the sound that gets played on their corresponding beat.
<video>

We would end up with X nodes and Y links 
This actually What i did to make the drums for the demo: 
<video>

But this is boring! We can do better, i iterate it on a couple of times and the least i could make it is 6 nodes and Y links:
<pic>

So lets see how we got there.
Well the obvious observation we can make is that there is a repeating 4-beat pattern for every note.
and the only thing that is changing is note that gets played 

sounds make to colors or resources as name them the code 
and what do link or transitions map to? well the map to changes between colors 

a cool quality of modeling languages is that they are modular, we don't to thing of the system and a whole 
we can subdivide it into sub problems that we can work on seperatly and tie together to form a solution for the system as whole 

so lets just consider the 4-beat pattern for only one note:
<video>

How many color changes do we need? 
we have two nodes: a high note and a low note 
from beat 3 to beat 4 from, we transition from low to high, so lets make a link for that: 
this link will assert that its starting link low and will perform coloring the destination node with high
<vid>

between the other two beat the note does not change, so do we need a transition for them?
well that a bit trick, lets imagine at a specific beat none of the transitions fire, would a transition fire at the next beat?
well no! because the state that the assertion are computed against did not change: we are at a deadlock!

so we will need a transition from a light node to another light note to keep the machine alive:
<gif>

change the asign color , gives us the rest if the notes 
so we now have 4 small modules that play 4 beats each now after finishing note we just need to transition to the next 
we can add a transition between every two consecutive notes, that assert that the starting node is low and performs sets the recieving node to high:
<vid>

cool! we are still at 16 links but now we only have 12 nodes 

now to the key observation that will get us to the 6 nodes: 
at the start of every 4-beat pattern, if we now 
what is the earliest moment at which we can know the color of these nodes when the needle head reaches them? this maps to (what is the largest post-condition we can perform giving the precondition of that transition)
lets move the responsibility of assigning the colors of those nodes to that transition 
and now what do think about these two what makes distinct from the others 
as the video plays, we can notice that those nodes stay unused whenever the note they are attached to is inactive 
what can we do about that?

The pattern is 4 beats long 

We don't have to think of the melody as a whole, but we can rather consider the first 4 

Model Editing

this is the pattern view of the runaway melody
the naive approach to modeling it in mariem is to set the approriate bpm and link each beat sequentially  (in the is actually what i did in the drums in the demo video)
this will result in 4 * 4 nodes + 4 * 4 links
but we can do better! we can do in just 6 nodes! and 4 * 2 + 4 + 1 links!

if we try to reduce the representation of the melody, it least amount of information needed to reproduce it
we will have 4 nodes, ! pattern

the coolest emmersient thing about mariem is that its very modular
since all the nodes follow the same structure 
we can try to model a single instance of that sturcture a 4 node 
then will try to tie things together to get the full pattern 

i tried different solutions 
this is the last one 

there are three color changes in the pattern 
dark light, light light and light dark 

so we have three transitions 
one giving light gives dark, one given light returns light and one given dark gives light 
now we need to tie them together 

the graph interpreter that gets run on the graph on every beat, defined by the bpm 
the get the desired melody, w need to remove the intermediary transitions 

nice 

why does it get stuck at the end and what can we do about it?
well in the way i implemented the graph editor, it doesnt let you define two  links with the same direction 
between two nodes 
i made this choice to simplfy the implementation and i found that just stacking lines wouldnt look nice
but as i try to model more stuff i found that it would be handy to have more than one link and it would sustationally reduce graph sizes

but still this should not be a hard limitation, just a small inconvience for now and we can 
mitigate it in various ways 

how can we sove this here?
well we just need to reinitialize the 7 to light after the dark gets played 
and to change a color, we need a transition 

the reason for this is the links are simple lines and the just stack

now will add a repeater node so it doesnt get stuck on the last 

now that was a nice tangent that i wanted to metion because it gives in neat idea on how mariem works
but now lets scrap that! we dont want the same note to repeate over and over 
well as we observed the structure of the nodes is the same 
the only thing that is different  is the color, and that is an indicator that we only need to add more links and nodes! since 
since the links can reuse the same  nodes by attributing to them the colors needed at a specific instance

lets start small, lets merge two notes 
we want the light of 8 to play after the dark of 7 
well this neatly translates to a link between 8 and 7 that asserts that 7 is dark and that performs turning 8 light 

we can even transition back from 8 to 7 

that's it! if we can transition between two notes we can transition between all of them!
we just need to add a transition between 8 and 9 and 9 and 10 ad 10 and 7 
and that's actually all we need to tie all the melody together

and voila!
now given this 
how small can you make the drums!
this tutorial is meet to get you familiar with the interface and how it works
i hope this gave you a good introduction on how 
but in case the concepts i meant to convey got lost in translation, i think its approriate to provide a formal definition of the syntax and semantics of the language 

how many nodes would we need if we had more transitions? well just two! and all the color change 
data would be embeded in the transitions 


Interesting implementation details:
  Physics - Collisions
    these are tall the formulas the code implementation,
    i actually did not know that so little formulas can take you a long way!

    to me the coolest thing about the implementation, was the collision detection!
    the first version was so slow, but i kept rolling with it, because i had more essential stuff i needed to iron out 

    but one day i woke up and  i said enough to the slowness!
    i read the collision detection page on wikipedia, it compares different algorithms
    i steadled for sweep and prune because it was the simplest, before read the other candidate was trees and it didnt fit the use case 

    and i actually implement the full algothrim 
    now imagine you have a set of complex object you wanna a check they collide 
    well check if two complex arbitrary (convex or concave) polygons it is not cheap enough to perform o(n**2) detection 
    
    the idea behind collision is to split it into phases one that narrows down the candidates that a specific node collides by elimiting unlikly collision  with this is called the broad phase and one that goes throught the likely collisies that passed the broad test and if they collide using the exact expesive formulas

    the sweep and prune algorithm is a broad phase algothrim, how does it eliminate unlikely collisions from being considered in the narrow phase? well it replaced each entity in the system by its containing rectangle, and then checks collisions between those rectangles, how do we check a collision between two rectangles? well they intersect on both, the x and y axis, cool that a sheep computation, but arent we still using o(n**2) which would be still slow as n becomes large?, well thats the neat part about snp, the problem can be reduced to checking for intersections between interval on a one D line, a leetcode problem that can be solved in o(nlog(n))! well we just sort the intervals by there lower end and than check collisions 

    As i got to implemets this i realised that i was dealing with simple cercle not complex shapes! 
    So instead of implementing the second sort phase i just iterate over the candidates check sphere to sphere collision using the formula 

    here are the performance comparasens before and after, and here is what is would look like for the screen to be filled with BALLS
  Miniaudio
  very cool library, but i was holding it wrong for so long time
  until i implemented the full runaway demo and it ran it only 60fps!!!
  at first i suspected that the physics was the problem, but as i looked more it
how i was using it 
how i use it now 
and it actually improved but i was not satisfied and i mean the sound stuturing was still noticable 
so i looked more into the call graph and untill i encountered mp3_decode and oh my! i was using compressed mp3 files and Miniaudio had to decode them every time i wanted to play a sound, so i migrated to using wav file and lived happly ever after  

  Project management: normilized devide coordinates
i make it so that you can save the positions of the nodes 
because i 
i made a random project generator 
i used nob walk dir! very cool stuff, i just copied this example and it nicely fit the use case 
i just has to add the core logic and make so changes 

  UI and layout
    the whole thing is implemented on top of oui.h a library i implemented en parallel and it just exposes two functions! more on it here 

Resources:
Known bugs
the beat break down is from here 
i couldnt find a sample pack for the sound in runaway 
so i just looked for break does only 
i found this one 
i downloaded it as an mp3 and opened it in audacity to get the bass and the horns  that play in the background when the beat drops 
since the drums where drowned in the other sounds 
i used audacity to chop the original song that kanye samped to make runaway 
i made a project that ground the different resources and attribtes colors to them 
i make it so that you can save the positions of the nodes 
because i 
i made a random project generator 
i used nob walk dir! very cool stuff, i just copied this example and it nicely fit the use case 
i just has to add the core logic and make so changes 
the data used in the examples is kinda large, so i packed it in a zip file and uploaded it hear 

