# General

- Something like this:
	- https://twitter.com/probabletrain/status/1260571939208388612
	- This would generate a set of default wall polygon entities
		- We can later use clipper for the user to be able to e.g. make a rectangular selection to split this polygon
			- so they can assign different prefabs to the walls
	- We could either write a parser or write a simplified tool
		- But it's enought that we introduce another object type: a polygonal wall
			- such that it is possible to carve and split it later
		- wall layer? lol
		- or just an object
	- Floors could later be tile layers just fine, the polygons will be drawn over them
	- We could still create tiled wall layers for nice transitions
	- or paint with tiles over the polygons

- The polygonal wall could just be another object type rendered over everything
	- the artist would choose how they'd like to go about creating wall layouts


- Set a reasonable minimum for a tile.
	- let's go and create a full blown tile architecture.
	- 

- Minimap

Let go of prefabs and per-variable overrides, for now.
Just have simple set of "initial" parameters to tweak.
We might have instantiation templates.

DUNGEON SOLDAT-LIKE MAPS ARE A COMPLETELY DIFFERENT CONCERN FROM grid-based maps.
They will be VERY RARELY intermixed.
We can implement dungeon-like layout planner later and for now just stick to squares and 45 * diagonals.

- What about stuff like a diagonally placed room with an axis-aligned floor?
	- Best would be for the game itself to somehow place the UVs perfectly, so that we don't have to trim these or render stuff over it.

- Let's use pen and pencil to visualize the usecases before we get to further planning, let alone implementation

- Later it will even be easier on the lighting algo if we just have a single entity for an L-shaped intra-obstacle

- Dilemma: One layout layer carved and bent on the go
	- or object-based
		- since the order of applicaton matters, we need several layers (walls, holes, intra-obstacles) that re-calculate the final layout every time
	- or just paint walls/floors right away? Dunno
	- somehow grid-tile-based?

- I'd be after something immediately visual rather than schematic
	- Well, it could still be distinct colors, like orange for walls in cs go and some neutral for floors

- Remember that layout in this game is very important.
	- We might consider the aesthetic pass, like with terrain brushes, a WHOLELY separate one
	- Players will be encouraged to first test out the layout with all those orange boxes and only later place details

- Wall brush
- Floor brush
- Just two for the layout tool
	- With labes like 128x128

- Automatically "paint" lay-outed floor and wall segments
	- Which will just place entities

- Supposing you were new to a map with a complicated layout, how would you make it THE EASIEST to say change a room from a diagonal to a square one?
	- Right, a brush. No one wants to place some stupid points.
	- Later we might still enable application of terrain by just plotting several points, useful for large-ass dungeon obstacles
	- Wall tool and floor tool that detect what kind of wall to place in order to continue
	- We might still want to have a separate layout file?

- We might even make procedural content filling based on an already made layout

- Floor chunks
	- Object based because they will later let us specify order simply

- Polygonal "fill" later

We want to have a layout tool that will easily let us create room-based axis-aligned layouts as well
as arbitrarily polygonal dungeons.

We might want to use an advanced polygon math library for stuff like additions, subtractions.

## Procedural layout generation?

That would be too evil... even so...

## File format

- arena.layout
	- Edited in a separate tab
	- Not selectable/editable in the normal content editor

- We might want to use this directly for radar rendering
	- The layout skeleton will also be shown in the arena selector next to the normal preview
