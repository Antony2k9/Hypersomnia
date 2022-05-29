The flavour and entity hierarchies let you tweak innumerable properties 
of the map's basic building blocks: the game objects.

This dialog has the most potential for customization.
Before we go any further, you have to gain a basic understanding 
of how Hypersomnia manages its game state.

## Theory: Flavour vs Entity

Summarizing from http://wiki.hypersomnia.xyz/entity_flavour

So that working with content is easier, and so that less memory is wasted,  
each game object is divided into two kinds of data:

- Flavour - state that is defined only once and is later shared by one or more entities. 
  The flavour's properties NEVER change during gameplay - except when the map designer tweaks values during a playtest - 
  and can only ever be modified from within this editor.
	For example: the muzzle velocity and the fire rate of a gun.
- Entity - state whose instance is needed by EACH and EVERY discernible object, since it constantly changes during gameplay.
	For example: the object's position or a flag signifying whether a gun's trigger is pressed, or the volume of its heat engine.

A game "object" is an entity and its flavour seen as a whole.

Except for entity positions, a map designer will be concerned with flavour properties 99% of the time.
Viewing entity properties is mostly useful for debugging in-game problems.

## Theory: Invariants vs Components

Invariants are constituent elements of a flavour.
Components are constituent elements of an entity.
That is the only difference.
Invariants and components of the corresponding types will always exist at the same time in a game object,
e.g. a sentience component will always be present alongside a sentience invariant.

## Theory: Entity type

There is a significant difference between this editor and general game creators like Unity.

Here, the memory layout of all entities and flavours is hardcoded, and thus, immutable.  
It means that there are only several possible combinations of invariants and components,
and you cannot manually add, remove or disable components/invariants at will.  

This is for several reasons:
- Performance. These assumptions let us employ some massive hacks to improve cache coherency,
  e.g. we already iterate all entities linearly, 
  but we could even make each component type reside in its own dedicated array.
- It is less error-prone. It would be very hard to ensure that each possible combination
  of invariants and components - the number of which grows exponentially - does not crash the game 
  or make it otherwise unplayable.
  
  We don't see much benefit in letting mappers create "camera objects that shoot bullets",
  or "spawnpoint markers that can also explode", because we're more pragmatic than idealistic.

  That doesn't mean that you can't introduce another type like this, as a C++ developer!
- It was way, way simpler and faster to code.

An entity type is, for example, a "Controlled character".
It is used to create playable characters. 
It will have components like a sentience, a rigid body, a fixture (which is another name for a "collider") 
and a crosshair, among many others.

Some kind of a wall, on the other hand, will have a "Plain sprited body" type.
It will have a rigid body and a collider, but no crosshair or sentience.

## GUI: Overview

Now that you know the terminology, let's get you started with the GUI itself.

If you only select a single entity, 
a unified view of its invariants and components will always be shown at once, for convenience.

If you select more than one entity, the GUI presents you with a hierarchy 
of all currently selected entities and their respective flavours, along with some widgets, like:

- A filter for quickly finding a desired flavour, or an entity of desired flavour.
- At the bottom, whether you want to view flavour or entity properties at the moment.
- Nodes for (un)folding flavours or entities of a particular type.

A flavour can have a name and a description.  
These appear in the game world whenever an entity is hovered with the mouse cursor.  

Simply unfold invariant and component nodes to edit them accordingly.
Whenever you tweak a property, a command will be spawned, 
and you'll be able to undo virtually any performed change.

If you want to see the history of changes for a specific field,
simply open up the History GUI (Alt+H) and write the property's name into the filter box.

The documentation of the purpose and behaviour of all properties of invariants and components 
will eventually be documented on wiki.hypersomnia.xyz.  
The names should, however, be more or less self-explanatory,
and you will probably learn most of this by way of experimentation.

## GUI: Multiple selections

There are checkboxes to the left of entity types, flavours and entities.

If you edit an entity or flavour that is currently checked, 
the change will propagate to all other currently checked entities or flavours.

If you edit an entity or flavour that is currently unchecked,
ONLY this one will be edited, even if other entities or flavours are currently checked.

Note that only flavours or entities of THE SAME type can be modified at once.
E.g. you can set identical fire rate to a dozen of gun types at the same time,
but you won't be able to set identical fire rate to a gun and a character, because, well,
a character does not have a fire rate.
	Note: Even though some invariants and components exist in two or more entity types,
	      for example - one could perfectly set identical collider density to a character and a wall -
	      it is still not possible to do due to implementation complexity. 
	      Maybe in the future we'll implement that.

If the value of a property is identical across the entire selection, 
its widget will have a natural, blue color.
If, on the other hand, it differs somewhere, e.g. 9 out of 10 selected guns have a fire interval of 100ms,
but the tenth has 110ms, the value of this property will be highlighted in orange.

## GUI: Ex and On buttons

You may have noticed "Ex(clude)" and "On(ly)" buttons next to flavours and entity types.
These let you quickly exclude entities of a given type or flavour from the current selection.

For example, if you wanted to only select all "Shootable weapons" on the scene,
you could firstly select all entities (Shift+1 and then Ctrl+A),
and then click "On" next to the "Shootable weapons" node.
Alternatively, if you wanted to select ALL entities EXCEPT all guns, you could press "Ex"
to remove only and exactly the "Shootable weapons" from the current selection.

You can do the same with flavours, not just entity types.

## Special widgets: asset identificators

Some widgets require you to choose a path to a resource, for example Sprite invariant->Image id.
A mini explorer will be spawned when you click on the combo box.

Assets from ONLY these locations will be shown on the list:
- the project-specific ``gfx`` and ``sfx`` folders,
- the official collections (content/official/gfx, content/official/sfx).

This is to ensure that your project folder has everything it needs to be opened on another machine.

The chosen image will be automatically imported if it has not yet been.

Additionally, if you alter Sprite invariant->Image id, the sprite's size will be updated to match the image size.
The flavour's name will also be automatically updated (although as an undoable command!),
since it is best to name flavours after the images they are associated with.
