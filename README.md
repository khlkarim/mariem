# mariem - A Visual Modeling Language

[![Watch the demo](https://img.youtube.com/vi/OVQGVA9cIro/maxresdefault.jpg)](https://youtu.be/OVQGVA9cIro)

## Building the executable

### Dependencies
I opted, whenever i could, to depend on **stb-style** or minimal libraries because they hugely simplify the build process.
You shouldn't worry about these, since their source is included in the project and compiles with it, but they are worth mentioning. I used:
- [nob.h](https://github.com/tsoding/nob.h): as a build and a logging system.
- [miniaudio.h](miniaud.io): used its high level API for audio playback.
- [tinyfiledialogs](https://tinyfiledialogs.sourceforge.net): for opening native file and message dialogs.
- [oui.h](https://github.com/khlkarim/oui.h): a tiny library for drawing 2D graphics that i made simultanouesly with this project.

The dependency that you **should** worry about however is **GLFW**, which is a dependency of oui.h. To build the executable, you will first have to build GLFW from its source, which is included as a git submodule under `./lib/glfw`. To build it you will need cmake. 

After installing cmake, just navigate to the `./lib/glfw` folder and run the following commands: 

```bash
git clone https://github.com/glfw/glfw.git . # if the directory is empty
cmake -S. -Bbuild 
cmake --build build
```

After this step, you should end up with a static library under `./build/src/libglfw3.a`.
If you face any issues building GLFW, [their official guide](https://www.glfw.org/docs/latest/compile.html) is a great resource.

Now, you just have to compile the nob executable and run it to build the main project: 

```bash
mkdir build # at the project's root
cc -o ./build/nob ./nob.c # where cc is your favorite c compiler
./build/nob # this will compile the project and create: ./build/main

# To compile a debug build of the project:
./build/nob main debug

# There is also a random-graph-generator that you can compile using this command:
./build/nob gen # This will create ./build/gen
```

>[!WARNING]
>The above process should be platform independent, but i have only tested it on my linux machine. To test the windows build, i used mingw to cross-compile to windows and then ran the executable using wine. It seemed to be working just fine, but the mingw-wine setup doesn't fully replicate the windows environment, so if you encounter any issues let me know.

## Usage 
The interaction with the app is keyboard centric, these are the main keybindings that let you do stuff:
|Key|Action|Description|
|---|---|---|
|N|Creates a [N]ode|This will create one node for every key press, pressing LEFT SHIFT along with N would let you spam-create nodes.|
|D|[D]eletes the selected entity|The entity can be either be a node or a link, to select an entity you just click it|
|S|[S]aves the current project to a file|this saves the BPM, nodes, links and color mappings to a txt file that you can later reload|
|L|[L]oads a saved project|This just parces the contents of a project file, and adds them to the current scene, without removing anything that is already there|

>[!WARNING]
>Currently, when you save a project, the paths to the sound files are saved as absolute paths. Until i fix this, if you want to share a project, make sure to edit the project's text file to make them relative.

There are other keybindings that i didn't mention in this list.
To see the full list, the keybindings are all defined as macros at the start of the `./src/main.c` file.
You can customize them by just changing those macros and then recompiling the app.

To make sure that everything is set up correctly, you can pick a file from the examples directory and load it as a project.
Pressing the space bar should make something happen.

>[!WARNING]
>The projects in the `./examples` directory point to sound files that should exist in `./data`, since these files would significantly inflate the size of the repo, i compressed them into a zip file and uploaded them [here](https://drive.google.com/file/d/1WjJifQdBB78LlJOsQPGhsAPZGIZZmTdZ/view?usp=sharing). If you download and unzip this folder in the project's root, the examples should work fine.

## Motivation
**What is mariem? And why am i calling it a language?**

To answer that, i think we first need to answer a deeper one first: **Why did i even make it in the first place?**

Well, have you ever tried using **FlStudio**?
Its a music production tool, more specifically a Digital Audio Interface (DAW), that lets you produce, mix, and master music.
And if you have ever tried it, you are probably familiar with its interface:

![Fl Studio Interface](https://github.com/user-attachments/assets/5cb31a2d-7fb8-4a92-9a80-70949d827356)

Its a great tool, but oh my is the interface **intimidating**! And it has so much stuff, more than i could ever need or want.

This is what pushed me to find a simpler way of making music, something that just implements my own mental model of a beat: sounds playing at discrete intervals of time.

But for a while i couldn't find an intuitive way of doing this. Until last semester, when i took a systems modeling course, it served as an introduction to topics like: **Petri nets**, **Colored Petri Nets** and **the B-method**. 

And there and then it finally clicked! What if we took the **colored directed graph** structure of a CPN, made the firing of the transitions **deterministic** and then considered every transition as an operation of a B-Machine, having its own **pre-condition** and **post-condition**...

Well, if we put all of this together with some sugar, spice, and everything nice, we'll get mariem!

![Sugar, spice and everything nice!](https://github.com/user-attachments/assets/af7af567-364e-4e78-8467-4e2aaa791b08)

**But how are we going to add determinism to the firing of the transitions?**

As you are building the graph, you will be able to select a subset of the nodes as starting nodes, from which the graph traversal will begin. 
You can do this by clicking a node to select it, and then pressing H to make it a starting node.

On each beat, and for every currently visited node, we check the pre-condition of every link connected to it, if that pre-condition is valid, the transition gets triggered:

![Starting nodes](https://github.com/user-attachments/assets/dfdbd9f9-d78b-497c-bc76-d17a6ef6da43)

**And how are we going to define these pre-conditions and post-conditions?**

Defining the initial coloring of the graph, defining the coloring asserted by a transition, and defining the coloring performed by a transition all have one thing in common: We are always coloring the same graph with the same nodes and links, the only thing that actually changes is what that coloring means.

This is a perfect use case for ![Model Editing](https://lazyvim-ambitious-devs.phillips.codes/course/chapter-2/): "Modes simply mean that different keystrokes mean different things depending on which mode is currently active."

There are 4 modes:

**Initialize mode**: In this mode you set the initial coloring of the graph (This is the default mode).

![Initialze mode](https://github.com/user-attachments/assets/c99e2cb6-f875-4056-900b-cd11bf07eed7)

**Assert mode**: If you click a link in Initialize mode and then press A, this will put you in assert mode, where you can edit the assert statement of the currently selected link.

![Assert mode](https://github.com/user-attachments/assets/3be85f18-9f0e-407d-9729-d5e6416c5321)

**Perform mode**: If you click a link in Initialize mode and then press P, this will put you in perform mode, where you can edit the perform statement of the currently selected link.

![Perform mode](https://github.com/user-attachments/assets/ac3b4893-7195-4d9b-ad63-99ae5da22589)

>[!WARNING]
>If you press A or P without selecting a link, nothing will happen. (which is bad UX, and i will fix this as soon as i can)

**Playing mode**: If you press SPACE the graph traversal will start. In this mode, you can still edit the active state of the graph and change to Assert and Perform modes without stopping the audio player.

![Playing mode](https://github.com/user-attachments/assets/2c53daaf-ca37-483c-a0d9-2a83eb3a3512)

If you press I in any of the above modes you will get back to **Initialize mode**.

## Tutorial: Lets make some music!

The purpose of this section is to get you familiar with the workflow of using the graph editor.
We are going to make the melody of [Runaway](https://open.spotify.com/track/3DK6m7It6Pw857FcQftMds) seen in the demo video!

Here is [a remake of the melody in FlStudio](https://www.youtube.com/watch?v=-32O_6eR6mY):

[FlStudio remake](https://github.com/user-attachments/assets/002c65af-8e37-4f18-8f1b-176c63c263c0)

**How would we represent this as a graph?**

Well since the transitions fire every beat, the trivial way of doing this is to make a simple loop where every node maps to a beat, and plays its corresponding note:

[Bruteforce](https://github.com/user-attachments/assets/c3f4fdff-9988-46fd-9fd2-f12c149d6c07)

We would end up with 16 nodes and 16 links.
And this is actually what i did to make the drums in the demo.

But this is boring! We can do better.
I iterated on this a couple of times, and i was able to get it to 6 nodes and 12 links:

[Reduced piano](https://github.com/user-attachments/assets/3321d444-2e94-4e9a-95c0-4c4b004d4cf3)

So lets see how we can get there.
Well, the obvious observation we can make is that there is a repeating 4-beat pattern for every note.
And the only thing that is changing is the note that gets played. 

A cool property of modeling languages is that they are modular, we don't need to think of the system as a whole.
We can rather subdivide it into sub-problems that we can work on independently and tie together at the end. 
So lets just consider the 4-beat pattern for only one note.

Conceptually, sounds map to colors and links map to color changes.
We have two notes: a high note and a low note => 2 colors.

**How many color changes do we need?**

From beat 3 to beat 4, we transition from a low note to a high note, so lets make a link for that.
This link will assert that its starting edge is a low note and will perform setting the destination edge to a high note:

![Low to high transition](https://github.com/user-attachments/assets/d1ccb384-87cc-4632-847c-0c6245ccc1c3)

Between the other two beats the note does not change, **do we need a transition for them?**

Well that's a bit tricky, lets consider the case where at a specific beat none of the transitions fire, would a transition fire at the next beat?
Well no! Because the state that didn't validate any of the assertions did not change from the previous beat to this one.
So to keep the graph traversal alive, we will need to add a transition that does not change any colors.

Now lets put all these together to form the 4-beat pattern:

[Each note separately](https://github.com/user-attachments/assets/3413a7ce-8cce-4584-89b3-4f4d2a3d3c39)

Changing the assigned color, gives us the rest of the notes.

We have 4 small modules that play 4 beats each. Now, after finishing a note, we just need to transition to the next. 
We can add a transition between every two consecutive notes that asserts that the previous note is done playing and transitions to the next one:

[Combined notes](https://github.com/user-attachments/assets/16d3f05e-bdd4-4101-8f7d-acf9781e62b4)

Cool! Yes, we are still at 16 links but we now only have 12 nodes.

Now to the key observation that will get us to just 6 nodes: 
Lets consider these nodes: 

![Annotated notes](https://github.com/user-attachments/assets/abee2824-92b6-4a02-9a08-d678838e0e3e)

What makes a pair special compared to the other pairs?
Well, nothing! When a pair is being traversed, the others are inactive.
If we try to reuse those nodes so that all the main nodes connect to the same pair, we'll get this:

[Reduced piano](https://github.com/user-attachments/assets/75eec5f2-c4c8-4435-8e63-3d6227473ad9)

And voila!

I hope this gave you a good idea on how everything works. 
You can play around with the drum sequence and see in how little entities you can represent it!

# Implementation detail: The physics system
To me, the coolest part of the implementation, was implementing collision detection!

![Dora the explorer](https://github.com/user-attachments/assets/b4bd55a0-a613-4282-8cae-bf4a6a5288fc)

The first version was the brute force approach of iterating over all the possible pairs of the nodes and checking if they are colliding using the formula: $||c_1 - c_2|| \le r_1 + r_2$. But this approach was too slow, it was bearly handling 600 nodes:

![Bruteforce Call Graph](https://github.com/user-attachments/assets/23eadf5f-854e-4b9b-b0cd-50dc11551626)

After reading the [collision detection wikipedia page](https://en.wikipedia.org/wiki/Collision_detection), which compares the applicability of different detection algorithms. I stealed for the Sweep and Prune algorithm, because it was the simplest and best fit for our limited use case.

And i didn't even implement the full algorithm! 
Why? Well, checking if two complex objects are colliding is not a cheap operation, but its a necessary one to get accurate results. That's why collision detection is generally split into two phases, one that narrows down the candidates to consider for a specific object: the broad phase, and one that goes through those candidates that passed the broad phase and performs the exact expensive check: the narrow phase.

The Sweep and Prune algorithm is a broad phase algorithm. How does it eliminate unlikely candidates?
It considers the bounding box of each object in the system, and checks for collisions between those boxes. To check if two boxes collide, we just need to make sure that they intersect on both the x and y axes. Since the axes are independent, this problem can be reduced to checking for intersections between intervals on a line: [A Leetcode problem](https://leetcode.com/problems/merge-intervals/description/) that can be solved in O(n*log(n)) by just sorting the intervals and then checking for intersections between the neighboring ones.

Step 1: We have to sort the boxes on the x-axis, then for each entity, we have to put the ones that intersect with it in a list: L.

Step 2: We have to sort the boxes on the y-axis, then for each entity, we remove from L the entities that don't intersect with it in the y-axes and keep the others. 
After this, we would end up with the entities that pass the broad phase.

But in step 2, since we are already iterating over the entities that intersect in the x-axes, and since we are dealing with simple circular object, whose exact check is not actually that expensive, why don't we just use the exact formula to eliminate the false positives on the x-axes? Well, that's exactly what i did! This is not an optimization however, since the overall complexity is still O(n * log(n)), but it significantly simplified the implementation.

Here is how the call-graph ended up after implementing Sweep and Prune:

![Sweep and prune call graph](https://github.com/user-attachments/assets/9a3690c8-8e26-4db9-a277-c1834081a57f)

We can now handle up to 3000 nodes! (There is a lot more to improve, but let's enjoy it for now)

[3000 nodes](https://github.com/user-attachments/assets/642c3f07-6411-47b2-8b39-756477d0f6b9)

## Final Note

If you've managed to get it working, i hope you've enjoyed playing around with the project. If you haven't, i would really appreciate if you report any issues you might have encountered, and i will try my best to get them fixed in time so that others don't get stuck on the same hurdles, and **thank you for your time!**
