---
title: ToDo bugs
hide_sidebar: true
permalink: todo_bugs
summary: Just a hidden scratchpad.
---

- fix enemy health bars visible outside of fov
	- we should just test a point against the visibility polygon no?

- fix crash:
	C:\projects\hypersomnia\src\augs\string\path_sanitization.cpp(506): FAILED:
	  REQUIRE( S::try_generate_sanitized_filename("훶훳힄훳챨?┖궸?) == "" )
	due to unexpected exception with message:
	  대상 멀티바이트 코드페이지에 유니코드 문자의 매핑이 없습니다.
- Not sure how possible, we don't even have this line of code lol
	- likely just prune non-asciis before trying to generate a safe fname 
	- but test on windows later if it crashes

- dont do nat traversal when replaying a demo

- shuncio bug
	- game crashed in main menu 

- Write unit tests for constant size string just in case

- It looks like the stuck udp transmissions were due to acks not going through because of too small sequence buffers for acked packets.
	- We WON'T have this problem normally because we're otherwise always sending at a tickrate of 60.
		- so the packets will go through.
		- (even with claarity it went through if we set bandwidth to 0)
		- So we shouldn't worry about initial state because these blocks will be small and always sent without additional bandwidth
	- However to not bloat the ack system + have adaptive bandwidth we could simply just send hash+index requests over the unreliable channel, given the file sizes (provided along with json file preferably)
	- And now I see that increasing these buffers really screws up performance (0.3ms->3ms for adapter advance, unacceptable)
	- We should really implement a simple protocol for requesting chunks, will be fun too
	- each chunk will have a small overhead for blake hash+sliceindex, so maybe 36 bytes? acceptable

- normalize the footstep sounds

- editor crashes when deleting lots (almost all) reources which are still in use

- fy twoj stary bug
    - Solved: The server didn't check for autosave in official maps, but sometimes autosave might end up there

    - there's an autosave, maybe this is the issue?
    - the other hash was the old file's and it is what got sent
    - maybe pepsik had the old one saved and had no autosave and requested the old hash 

- Billan crash
	- fixed: randomization system was mutable; and was thus used by multiple threads where it called try_emplace on walk states.
		- this is a prime example of how mutable can bite us in the ass, so let's not do this in rendering code because it is multithreaded

- if server is down after update check if removing the single second delay between interrupt and restart didn't break something
	- it was put in restart_servers for some reason but I don't recall why

- caki crash
	- changing fy minilab reloaded remake to de_labs2
		- vars was null
	- message.txt 
- lopa crash
	- when pressing OK on renaming gifs
	- we have logs on discord


- Weapon collider is fucked when it's in the other hand


We have a demo file


- don't autolaunch on the replayed demo!!!!! because it might crash

- on de_duel_practice, we had an openal error because of a reseek_to_sync_if_needed on a warmup theme
	- alSourcef was called with an out of range value, but I'm not sure how it's possible since we clamp the value always to the duration
	- However simply setting stop (stop is always valid for a source) fixes the issue for now, but I'm not sure how it happened in the first place

- crash on bigilab: we should be able to reproduce it with a demo on windows

- linux bug: neon silhouettes can be seen behind player, probably something to do with drivers
	- Solved (NO IDEA WHY): simply let neon fow draw first, then draw the diffuse fog of war

	- It's a problem with gl_FragCoord: probably stencil on another fbo is somehow flipped
	- To reverse the problem, one can put the following in fog_of_war.fsh: layout(origin_upper_left) in vec4 gl_FragCoord; 
		- although only with higher glsl version
	- didn't happen before, very probable it's a driver bug

- Fixing wallbangs and wall teleportation
    - A short raycast sensor in front of the player
        - Detect all convex shapes of walls in front of him
        - Just take all vertices of all detected convexes
        - And create a single trapezoid body
    - Don't do raycast
        - simply query the  right before the physics step (after applying the crosshair's direction to player rotation)

- There was a crash due to dead entity in driver setup finding a component
	- possible cause is that the gun system destroys item in chamber, but technically they should not trigger END_CONTACT
	as they are devoid of... hey, actually that might be because we still see those entities even if they are in backpack. 
	In that case, if the owner body of an item chamber is badly determined, it might make sense that it crashes
	- ensure that the posted subject in contact listener is alive, but that is most likely the case since we get physical components from them anyway
		- but ensures won't hurt

# Disregarded

- Investigate the mysterious hotbar crash
	- Happened when we finished drag and dropping on hotbar 
	- All buttons on hotbar were assigned
	- The item we dragged was hidden in backpack and *possibly* unassigned to hotbar yet
	- NEW: The crash happened EXACTLY when pressing an AWP that was in the backpack
		- But it was presuambly not in the hotbar since we pressed it
	- haven't seen for a while and inventory will be reworked anyway

- NAT traversal does not always work with our symmetric port-sensitive
	- Working connections:
		- filemon -> Billan
		- Pythagoras <-> Billan
		- Pythagoras <-> Shuncio
		- Pythagoras -> cold dimensions
		- Pythagoras -> Wojteg
	- Not working:
		- filemon <-> Pythagoras
	- not so important, and it's from a long time ago, generally it works if the nat does not randomize ports

- GCC relwithdebinfo does not properly set the physical shapes
	 - ?


- Possibly disallow some editor ops when in gameplay/gui mode?
	- E.g. there was a crash after instantiating cause the world cursor pos could not be found in gameplay mode
	- new editor will solve this

- fix freetype charsets
	- ?

# Done

- fix dinkydinky crash
	- stb_image bug fixed by this PR https://github.com/nothings/stb/pull/1404

- WE DON'T WANT TO CLEAR client pending entropies on desync because it might happen mid-gameplay. We want to apply all actions applied during resync.
- Note client begins sending net entropies as soon as receiving RESUME_SOLVABLES but clears them soon after when they receive initial snapshot.
	- The server thinks these entropies came after clearing them.
	- The server does not know which of the arrived client entropies are post client-side clear.
- Note it's different with RESYNC command on desync.
	- So we shouldn't clear solvable stream there.
	- ::RESYNC_ARENA arrives synchronously with other client-entropies.
		- so the server knows that all entropies post-resync are after clearing.
	- However we don't want to clear the buffer here because resync request might arrive mid-gameplay.

- Problem: solvable stream might be stopped at an arbitrary point when we start downloading externally
	- fixed with FINISHED_DOWNLOADING notification
	- worked earlier because the client was sending packets regularly not just keepalives
	- some entropies might not get through if you download externally and only send keepalive packets
		- then they suddenly can appear once you start exchanging packets regularly
	- in case of direct downloads this goes over the reliable channel
		- so the first file packet can only arrive after the initial solvable gets through
		- which is why we always end with in-game


- all_necessary_sounds might cause a crash on destruct too

- remember to fix your system time before hosting
	- fix that damn connect token

- wrong interpolation values in client setup
	- But somehow only for fy_minilab
		- It was probably generated with 128 so the solvable might still have 128 written into it
		- but only predicted cosmos sometimes shows 128, referential always has 60
			- how is it possible if the only source of state for predicted is the referential?
				- maybe referential is somehow updated later? And predicted is only rewritten on mismatch afaik
				- yeah first both are read from disk, only then is initial state applied
					- after reading from disk both should have 128
					- then upon receiving initial, is predicted properly set to 60?
- warmup not played when switching maps sometimes and still spectating (in release)
	- solved, due to the above

- Wandering pixels crash
	- Fixed by eliminating Data race, see the comments in audiovisual_state.cpp

- Server crash from 11.01.2023 at 01:24 - fixed with *11b30d931246caa7e0cce04d7e83756eb33acd53*
	- Kartezjan then najsilniejszy programista C++
	- It crashed before successfully sending the message to Telegram but after successfully sending the Discord one
	- Actually rather peculiar, if both lived enough for the discord message to be properly sent, there's no reason telegram one wouldn't eventually arrive, must've been problem elsewhere 
		- I've also seen kartezjan for a bit so it must've sent me some data
	- Maybe httplib is not actually thread safe
		- Just in case I'd update it and merge the two webhook handlers into a single lambda

- Enabling HRTF in debug mode crashes the game but there's no core or segfault or anything
	- it just quits
	- Fixed by updating OpenAL

- Server sent entropy too early?
    - server wasn't pausing the solvable stream for downloading clients when sending entropies
- TLSF crashes after sending 9 files
    - Maybe we're not freeing the blocks properly
    - Probably an issue with tlsf, wouldn't be surprised if it was because of differing sizes for files, at least one block was remaining per each size
        - which is why we need to use default allocator for our block messages because their sizes will be nondeterministic
            - this will also free our per-client memory

- demo files might be fucked up during sessions with downloaded maps
	- they must correctly remember the full state snapshots
	- It was just a matter of ignoring file payloads

- When comparing dumped solvables - aligned storage DOES NOT CALL CONSTRUCTORS !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    - this is why you get differing results when comparing binary .solv files
    - we might zero it out for investigations
    - we could also use const size vector to zero out once we clear fixtures etc to look for the crash
	- although std::vector::clear would normally trigger a sigsegv too


- Desync with hajt
	- Try playing it on windows
	- Happens when damage is dealt for some reason
		- couldnt physics break at these bullet speeds?
	- Perhaps rapidjson is a source of indeterminism when loading doubles/floats
- The desync happened with claarity too on a very simple map so it makes me think it's an issue with how it's loaded from json

- Crash due to yojimbo_assert( sentPacket );
	- Only in debug build though
	- This doesn't really break anything if we just comment it out and let it go
		- this is basically because we don't send any packets on all the other channels for a while so they have m_sequence = 0, and they are suddenly greeted with a very large sequence number
		- the question being, if it's not inserted into the sequence buffer, how the hell is it even getting acked
			- so why does this just work?
		LOG_NVPS(sequence,m_sentPackets->GetSequence(), m_sentPackets->GetSize());
	- looks like the sequence number is increased by just acking the fragments, it won't help to withhold file requests
- Found culprit:
	auto step_input = logic_step_input { scene.world, entropy, solve_settings() };
	- this caused fish to not simulate during first step which later led to a desync
- Enough to change to:
	auto settings = solve_settings();
	auto step_input = logic_step_input { scene.world, entropy, settings };
- silly C++ just let us literally compile UB..
	- this would normally be catched by static analysis so we should do this

- Might also later fix the direct/indirect memory leaks reported by address sanitizer
	- done for box2d


## SOLVED - crash when cloning entities

Happened:
- when quick testing arena5, probably due to item deletion?
- On claarity's server, again the same similar bug

Solved:

- SIGSEGV was due to pool reallocating and invalidating all entity handles and component references.

- Note the pool didn't actually call reserve but it was objects.push_back that did it
	- Most probably as the pool was read from the network it didn't reserve a capacity for objects vector
	- But it read indirectors vector and indirectors.size() is taken as pool's capacity() so it didn't need to explicitly reserve

## Rest

- why prod-debug doesn't see details.lua? wrong cwd?
    - was because of build in console mode

- If we just 
					if (ce->other->m_last_teleport_progress_timestamp == now) {
						continue;
					}
    - then if two portals overlap (background one and a normal one)
        - one can "steal" from the other
    - on the other hand we need this counter to prevent contacts from re-beginning entering after teleporting (which only changes transform in physics_system but old contacts are still there)

# Done

- define-out the statically allocate variables because otherwise we get things like this:
	#0  visible_entities::register_visible(cosmos const&, entity_id) ()
	[Current thread is 1 (Thread 0x7fc43e3476c0 (LWP 50031))]
	#0  visible_entities::register_visible(cosmos const&, entity_id) ()
	#1  augs::scope_guard<visible_entities::acquire_physical(visible_entities_query)::$_1>::~scope_guard() ()
	#2  visible_entities::acquire_physical(visible_entities_query) ()
	#3  visible_entities::reacquire_all(visible_entities_query) ()
	#4  work(int, char const* const*)::$_34::operator()(augs::audio_renderer const*, augs::delta, double, config_lua_table const&) const ()
	#5  _ZNSt3__116__variant_detail12__visitation6__base12__dispatcherIJLm1EEE10__dispatchB6v15007IONS1_9__variant15__value_visitorIZZ4workiPKPKcENK3$_2clIZZ4workiS
	B_ENK4$_36clEPKN4augs14audio_rendererENSF_5deltaERK17input_pass_resultEUlRT_E_EEDcSN_EUlSO_E_EEJRNS0_6__baseILNS0_6_TraitE1EJ16test_scene_setup12editor_setup22p
	roject_selector_setup12client_setup12server_setupEEEEEEDcSN_DpT0_ ()
	#6  work(int, char const* const*)::$_40::operator()() const::{lambda()#1}::operator()() const ()
	#7  void* std::__1::__thread_proxy[abi:v15007]<std::__1::tuple<std::__1::unique_ptr<std::__1::__thread_struct, std::__1::default_delete<std::__1::__thread_struc
	t> >, work(int, char const* const*)::$_40> >(void*) ()
	#8  ?? () from /usr/lib/libc.so.6
	#9  ?? () from /usr/lib/libc.so.6




# The mysterious sound_system::fade_sources crash. Findings.

We had another one of these with caki. Full backtrace is as follows:

(gdb) bt full
#0  std::__1::__hash_table<std::__1::__hash_value_type<collision_cooldown_key, sound_system::collision_sound_succession>, std::__1::__unordered_map_hasher<colli
sion_cooldown_key, std::__1::__hash_value_type<collision_cooldown_key, sound_system::collision_sound_succession>, std::__1::hash<collision_cooldown_key>, std::_
_1::equal_to<collision_cooldown_key>, true>, std::__1::__unordered_map_equal<collision_cooldown_key, std::__1::__hash_value_type<collision_cooldown_key, sound_s
ystem::collision_sound_succession>, std::__1::equal_to<collision_cooldown_key>, std::__1::hash<collision_cooldown_key>, true>, std::__1::allocator<std::__1::__h
ash_value_type<collision_cooldown_key, sound_system::collision_sound_succession> > >::remove (this=this@entry=, __p=...) at /usr/lib/llvm-14/bin/../include/c++/
v1/__hash_table:2555
        __cn = 
        __bc = 23
        __chash = 8
        __pn = 
#1  std::__1::__hash_table<std::__1::__hash_value_type<collision_cooldown_key, sound_system::collision_sound_succession>, std::__1::__unordered_map_hasher<colli
sion_cooldown_key, std::__1::__hash_value_type<collision_cooldown_key, sound_system::collision_sound_succession>, std::__1::hash<collision_cooldown_key>, std::_
_1::equal_to<collision_cooldown_key>, true>, std::__1::__unordered_map_equal<collision_cooldown_key, std::__1::__hash_value_type<collision_cooldown_key, sound_s
ystem::collision_sound_succession>, std::__1::equal_to<collision_cooldown_key>, std::__1::hash<collision_cooldown_key>, true>, std::__1::allocator<std::__1::__h
ash_value_type<collision_cooldown_key, sound_system::collision_sound_succession> > >::erase (this=, __p=...) at /usr/lib/llvm-14/bin/../include/c++/v1/__hash_ta
ble:2486
        __np = 
        __r = {__node_ = }
#2  std::__1::unordered_map<collision_cooldown_key, sound_system::collision_sound_succession, std::__1::hash<collision_cooldown_key>, std::__1::equal_to<collisi
on_cooldown_key>, std::__1::allocator<std::__1::pair<collision_cooldown_key const, sound_system::collision_sound_succession> > >::erase (this=, __p=...) at /usr
/lib/llvm-14/bin/../include/c++/v1/unordered_map:1339
No locals.
#3  std::__1::__libcpp_erase_if_container<std::__1::unordered_map<collision_cooldown_key, sound_system::collision_sound_succession, std::__1::hash<collision_coo
ldown_key>, std::__1::equal_to<collision_cooldown_key>, std::__1::allocator<std::__1::pair<collision_cooldown_key const, sound_system::collision_sound_successio
n> > >, sound_system::fade_sources(augs::audio_renderer const&, augs::delta)::$_17>(std::__1::unordered_map<collision_cooldown_key, sound_system::collision_soun
d_succession, std::__1::hash<collision_cooldown_key>, std::__1::equal_to<collision_cooldown_key>, std::__1::allocator<std::__1::pair<collision_cooldown_key cons
t, sound_system::collision_sound_succession> > >&, sound_system::fade_sources(augs::audio_renderer const&, augs::delta)::$_17&) (__c=..., __pred=...) at /usr/li
b/llvm-14/bin/../include/c++/v1/__iterator/erase_if_container.h:30
        __iter = {__i_ = {__node_ = }}
        __last = {__i_ = {__node_ = }}
        __old_size = <optimized out>
        __iter = <optimized out>
#4  std::__1::erase_if<collision_cooldown_key, sound_system::collision_sound_succession, std::__1::hash<collision_cooldown_key>, std::__1::equal_to<collision_co
oldown_key>, std::__1::allocator<std::__1::pair<collision_cooldown_key const, sound_system::collision_sound_succession> >, sound_system::fade_sources(augs::audi
o_renderer const&, augs::delta)::$_17>(std::__1::unordered_map<collision_cooldown_key, sound_system::collision_sound_succession, std::__1::hash<collision_cooldo
wn_key>, std::__1::equal_to<collision_cooldown_key>, std::__1::allocator<std::__1::pair<collision_cooldown_key const, sound_system::collision_sound_succession> 
> >&, sound_system::fade_sources(augs::audio_renderer const&, augs::delta)::$_17) (__c=..., __pred=...) at /usr/lib/llvm-14/bin/../include/c++/v1/unordered_map:
1869
No locals.
#5  sound_system::fade_sources (this=, renderer=..., dt=...) at /home/runner/work/Hypersomnia/Hypersomnia/src/view/audiovisual_state/systems/sound_system.cpp:97
6
No locals.
#6  audiovisual_state::advance(audiovisual_advance_input)::$_11::operator()() const (this=) at /home/runner/work/Hypersomnia/Hypersomnia/src/view/audiovisual_st
ate/audiovisual_state.cpp:316
        viewed_character = {<misc_mixin<basic_entity_handle<true> >> = {<No data fields>}, <inventory_mixin<basic_entity_handle<true> >> = {static is_const = tr
ue}, <physics_mixin<basic_entity_handle<true> >> = {static is_const = true}, <relations_mixin<basic_entity_handle<true> >> = {static is_const = true}, <spatial_
properties_mixin<basic_entity_handle<true> >> = {<No data fields>}, ptr = , owner = , raw_id = {raw = {indirection_index = 1, version = 1}, type_id = {static de
ad_value = 4294967295, index = 1, static max_index_v = <optimized out>}}, static is_typed = false}
        scope = {exit_function = {<std::__1::__optional_move_assign_base<(lambda at /home/runner/work/Hypersomnia/Hypersomnia/src/augs/misc/measurements.h:208:2
7), false>> = {<std::__1::__optional_copy_assign_base<(lambda at /home/runner/work/Hypersomnia/Hypersomnia/src/augs/misc/measurements.h:208:27), false>> = {<std
::__1::__optional_move_base<(lambda at /home/runner/work/Hypersomnia/Hypersomnia/src/augs/misc/measurements.h:208:27), true>> = {<std::__1::__optional_copy_base
<(lambda at /home/runner/work/Hypersomnia/Hypersomnia/src/augs/misc/measurements.h:208:27), true>> = {<std::__1::__optional_storage_base<(lambda at /home/runner
/work/Hypersomnia/Hypersomnia/src/augs/misc/measurements.h:208:27), false>> = {<std::__1::__optional_destruct_base<(lambda at /home/runner/work/Hypersomnia/Hype
rsomnia/src/augs/misc/measurements.h:208:27), true>> = {{__null_state_ = -112 '\220', __val_ = {<No data fields>}}, __engaged_ = true}, <No data fields>}, <No d
ata fields>}, <No data fields>}, <No data fields>}, <No data fields>}, <std::__1::__sfinae_ctor_base<true, true>> = {<No data fields>}, <std::__1::__sfinae_assi
gn_base<false, false>> = {<No data fields>}, <No data fields>}}
        interpol = {enabled = true, id_to_integerize = {raw = {indirection_index = 1, version = 1}, type_id = {static dead_value = 4294967295, index = 1, static
 max_index_v = <optimized out>}}}
        command_buffers = <error reading variable: value of type `augs::audio_command_buffers' requires 96256 bytes, which is more than max-value-size>
        ear = <optimized out>
#7  std::__1::__invoke<audiovisual_state::advance(audiovisual_advance_input)::$_11&>(audiovisual_state::advance(audiovisual_advance_input)::$_11&) (__f=...) at 
/usr/lib/llvm-14/bin/../include/c++/v1/type_traits:3640
No locals.
#8  std::__1::__invoke_void_return_wrapper<void, true>::__call<audiovisual_state::advance(audiovisual_advance_input)::$_11&>(audiovisual_state::advance(audiovis
ual_advance_input)::$_11&) (__args=...) at /usr/lib/llvm-14/bin/../include/c++/v1/__functional/invoke.h:61
No locals.
#9  std::__1::__function::__alloc_func<audiovisual_state::advance(audiovisual_advance_input)::$_11, std::__1::allocator<audiovisual_state::advance(audiovisual_a
dvance_input)::$_11>, void ()>::operator()() (this=) at /usr/lib/llvm-14/bin/../include/c++/v1/__functional/function.h:180
No locals.
#10 std::__1::__function::__func<audiovisual_state::advance(audiovisual_advance_input)::$_11, std::__1::allocator<audiovisual_state::advance(audiovisual_advance
_input)::$_11>, void ()>::operator()() (this=) at /usr/lib/llvm-14/bin/../include/c++/v1/__functional/function.h:354
No locals.
#11 std::__1::__function::__value_func<void ()>::operator()() const (this=) at /usr/lib/llvm-14/bin/../include/c++/v1/__functional/function.h:507
No locals.
#12 std::__1::function<void ()>::operator()() const (this=) at /usr/lib/llvm-14/bin/../include/c++/v1/__functional/function.h:1184
No locals.
#13 augs::thread_pool::make_continuous_worker()::{lambda()#1}::operator()() const (this=this@entry=) at /home/runner/work/Hypersomnia/Hypersomnia/src/augs/templ
ates/thread_pool.h:62
        task = {<std::__1::__function::__maybe_derive_from_unary_function<void ()>> = {<No data fields>}, <std::__1::__function::__maybe_derive_from_binary_func
tion<void ()>> = {<No data fields>}, __f_ = {__buf_ = {__lx = '\000' <repeats 31 times>}, __f_ = }}
#14 std::__1::__invoke<augs::thread_pool::make_continuous_worker()::{lambda()#1}>(augs::thread_pool::make_continuous_worker()::{lambda()#1}&&) (__f=...) at /usr
/lib/llvm-14/bin/../include/c++/v1/type_traits:3640
No locals.
#15 std::__1::__thread_execute<std::__1::unique_ptr<std::__1::__thread_struct, std::__1::default_delete<std::__1::__thread_struct> >, augs::thread_pool::make_co
ntinuous_worker()::{lambda()#1}>(std::__1::tuple<std::__1::unique_ptr<std::__1::__thread_struct, std::__1::default_delete<std::__1::__thread_struct> >, augs::th
read_pool::make_continuous_worker()::{lambda()#1}>&, std::__1::__tuple_indices<>) (__t=...) at /usr/lib/llvm-14/bin/../include/c++/v1/thread:282
--Type <RET> for more, q to quit, c to continue without paging--
No locals.
#16 std::__1::__thread_proxy<std::__1::tuple<std::__1::unique_ptr<std::__1::__thread_struct, std::__1::default_delete<std::__1::__thread_struct> >, augs::thread
_pool::make_continuous_worker()::{lambda()#1}> >(void*) (__vp=) at /usr/lib/llvm-14/bin/../include/c++/v1/thread:293
        __p = {__ptr_ = {<std::__1::__compressed_pair_elem<std::__1::tuple<std::__1::unique_ptr<std::__1::__thread_struct, std::__1::default_delete<std::__1::__
thread_struct> >, (lambda at /home/runner/work/Hypersomnia/Hypersomnia/src/augs/templates/thread_pool.h:46:11)> *, 0, false>> = {__value_ = }, <std::__1::__comp
ressed_pair_elem<std::__1::default_delete<std::__1::tuple<std::__1::unique_ptr<std::__1::__thread_struct, std::__1::default_delete<std::__1::__thread_struct> >,
 (lambda at /home/runner/work/Hypersomnia/Hypersomnia/src/augs/templates/thread_pool.h:46:11)> >, 1, true>> = {<std::__1::default_delete<std::__1::tuple<std::__
1::unique_ptr<std::__1::__thread_struct, std::__1::default_delete<std::__1::__thread_struct> >, (lambda at /home/runner/work/Hypersomnia/Hypersomnia/src/augs/te
mplates/thread_pool.h:46:11)> >> = {<No data fields>}, <No data fields>}, <No data fields>}}
#17 ?? () from /usr/lib/libc.so.6
No symbol table info available.
#18 ?? () from /usr/lib/libc.so.6
No symbol table info available.

- This problem finally disappeared when we switched to LLVM 16.


- another crash - when we died after killing ourselves with a grenade

This GDB supports auto-downloading debuginfo from the following URLs:
  <https://debuginfod.archlinux.org>
Debuginfod has been disabled.
To make this setting permanent, add 'set debuginfod enabled off' to .gdbinit.
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/usr/lib/libthread_db.so.1".
Core was generated by `/home/pbc/Downloads/forlinux/Hypersomnia'.
Program terminated with signal SIGSEGV, Segmentation fault.
#0  std::__1::__hash_table<std::__1::__hash_value_type<collision_cooldown_key, sound_system::collision_sound_succession>, std::__1::__unordered_map_hasher<colli
sion_cooldown_key, std::__1::__hash_value_type<collision_cooldown_key, sound_system::collision_sound_succession>, std::__1::hash<collision_cooldown_key>, std::_
_1::equal_to<collision_cooldown_key>, true>, std::__1::__unordered_map_equal<collision_cooldown_key, std::__1::__hash_value_type<collision_cooldown_key, sound_s
ystem::collision_sound_succession>, std::__1::equal_to<collision_cooldown_key>, std::__1::hash<collision_cooldown_key>, true>, std::__1::allocator<std::__1::__h
ash_value_type<collision_cooldown_key, sound_system::collision_sound_succession> > >::remove (this=this@entry=, __p=...) at /usr/lib/llvm-14/bin/../include/c++/
v1/__hash_table:2555
2555    /usr/lib/llvm-14/bin/../include/c++/v1/__hash_table: No such file or directory.
[Current thread is 1 (Thread 0x7f4ddb7fe6c0 (LWP 46855))]
#0  std::__1::__hash_table<std::__1::__hash_value_type<collision_cooldown_key, sound_system::collision_sound_succession>, std::__1::__unordered_map_hasher<colli
sion_cooldown_key, std::__1::__hash_value_type<collision_cooldown_key, sound_system::collision_sound_succession>, std::__1::hash<collision_cooldown_key>, std::_
_1::equal_to<collision_cooldown_key>, true>, std::__1::__unordered_map_equal<collision_cooldown_key, std::__1::__hash_value_type<collision_cooldown_key, sound_s
ystem::collision_sound_succession>, std::__1::equal_to<collision_cooldown_key>, std::__1::hash<collision_cooldown_key>, true>, std::__1::allocator<std::__1::__h
ash_value_type<collision_cooldown_key, sound_system::collision_sound_succession> > >::remove (this=this@entry=, __p=...) at /usr/lib/llvm-14/bin/../include/c++/
v1/__hash_table:2555
#1  std::__1::__hash_table<std::__1::__hash_value_type<collision_cooldown_key, sound_system::collision_sound_succession>, std::__1::__unordered_map_hasher<colli
sion_cooldown_key, std::__1::__hash_value_type<collision_cooldown_key, sound_system::collision_sound_succession>, std::__1::hash<collision_cooldown_key>, std::_
_1::equal_to<collision_cooldown_key>, true>, std::__1::__unordered_map_equal<collision_cooldown_key, std::__1::__hash_value_type<collision_cooldown_key, sound_s
ystem::collision_sound_succession>, std::__1::equal_to<collision_cooldown_key>, std::__1::hash<collision_cooldown_key>, true>, std::__1::allocator<std::__1::__h
ash_value_type<collision_cooldown_key, sound_system::collision_sound_succession> > >::erase (this=, __p=...) at /usr/lib/llvm-14/bin/../include/c++/v1/__hash_ta
ble:2486
#2  std::__1::unordered_map<collision_cooldown_key, sound_system::collision_sound_succession, std::__1::hash<collision_cooldown_key>, std::__1::equal_to<collisi
on_cooldown_key>, std::__1::allocator<std::__1::pair<collision_cooldown_key const, sound_system::collision_sound_succession> > >::erase (this=, __p=...) at /usr
/lib/llvm-14/bin/../include/c++/v1/unordered_map:1339
#3  std::__1::__libcpp_erase_if_container<std::__1::unordered_map<collision_cooldown_key, sound_system::collision_sound_succession, std::__1::hash<collision_coo
ldown_key>, std::__1::equal_to<collision_cooldown_key>, std::__1::allocator<std::__1::pair<collision_cooldown_key const, sound_system::collision_sound_successio
n> > >, sound_system::fade_sources(augs::audio_renderer const&, augs::delta)::$_17>(std::__1::unordered_map<collision_cooldown_key, sound_system::collision_soun
d_succession, std::__1::hash<collision_cooldown_key>, std::__1::equal_to<collision_cooldown_key>, std::__1::allocator<std::__1::pair<collision_cooldown_key cons
t, sound_system::collision_sound_succession> > >&, sound_system::fade_sources(augs::audio_renderer const&, augs::delta)::$_17&) (__c=..., __pred=...) at /usr/li
b/llvm-14/bin/../include/c++/v1/__iterator/erase_if_container.h:30
#4  std::__1::erase_if<collision_cooldown_key, sound_system::collision_sound_succession, std::__1::hash<collision_cooldown_key>, std::__1::equal_to<collision_co
oldown_key>, std::__1::allocator<std::__1::pair<collision_cooldown_key const, sound_system::collision_sound_succession> >, sound_system::fade_sources(augs::audi
o_renderer const&, augs::delta)::$_17>(std::__1::unordered_map<collision_cooldown_key, sound_system::collision_sound_succession, std::__1::hash<collision_cooldo
wn_key>, std::__1::equal_to<collision_cooldown_key>, std::__1::allocator<std::__1::pair<collision_cooldown_key const, sound_system::collision_sound_succession> 
> >&, sound_system::fade_sources(augs::audio_renderer const&, augs::delta)::$_17) (__c=..., __pred=...) at /usr/lib/llvm-14/bin/../include/c++/v1/unordered_map:
1869
#5  sound_system::fade_sources (this=, renderer=..., dt=...) at /home/runner/work/Hypersomnia/Hypersomnia/src/view/audiovisual_state/systems/sound_system.cpp:97
6
#6  audiovisual_state::advance(audiovisual_advance_input)::$_13::operator()() const (this=) at /home/runner/work/Hypersomnia/Hypersomnia/src/view/audiovisual_st
ate/audiovisual_state.cpp:308
#7  audiovisual_state::advance(audiovisual_advance_input)::$_14::operator()() const (this=) at /home/runner/work/Hypersomnia/Hypersomnia/src/view/audiovisual_st
ate/audiovisual_state.cpp:317
#8  std::__1::__invoke<audiovisual_state::advance(audiovisual_advance_input)::$_14&>(audiovisual_state::advance(audiovisual_advance_input)::$_14&) (__f=...) at 
/usr/lib/llvm-14/bin/../include/c++/v1/type_traits:3640
#9  std::__1::__invoke_void_return_wrapper<void, true>::__call<audiovisual_state::advance(audiovisual_advance_input)::$_14&>(audiovisual_state::advance(audiovis
ual_advance_input)::$_14&) (__args=...) at /usr/lib/llvm-14/bin/../include/c++/v1/__functional/invoke.h:61
#10 std::__1::__function::__alloc_func<audiovisual_state::advance(audiovisual_advance_input)::$_14, std::__1::allocator<audiovisual_state::advance(audiovisual_a
dvance_input)::$_14>, void ()>::operator()() (this=) at /usr/lib/llvm-14/bin/../include/c++/v1/__functional/function.h:180
#11 std::__1::__function::__func<audiovisual_state::advance(audiovisual_advance_input)::$_14, std::__1::allocator<audiovisual_state::advance(audiovisual_advance
_input)::$_14>, void ()>::operator()() (this=) at /usr/lib/llvm-14/bin/../include/c++/v1/__functional/function.h:354
#12 std::__1::__function::__value_func<void ()>::operator()() const (this=) at /usr/lib/llvm-14/bin/../include/c++/v1/__functional/function.h:507
#13 std::__1::function<void ()>::operator()() const (this=) at /usr/lib/llvm-14/bin/../include/c++/v1/__functional/function.h:1184
#14 augs::thread_pool::make_continuous_worker()::{lambda()#1}::operator()() const (this=this@entry=) at /home/runner/work/Hypersomnia/Hypersomnia/src/augs/templ
ates/thread_pool.h:62
#15 std::__1::__invoke<augs::thread_pool::make_continuous_worker()::{lambda()#1}>(augs::thread_pool::make_continuous_worker()::{lambda()#1}&&) (__f=...) at /usr
/lib/llvm-14/bin/../include/c++/v1/type_traits:3640
#16 std::__1::__thread_execute<std::__1::unique_ptr<std::__1::__thread_struct, std::__1::default_delete<std::__1::__thread_struct> >, augs::thread_pool::make_co
ntinuous_worker()::{lambda()#1}>(std::__1::tuple<std::__1::unique_ptr<std::__1::__thread_struct, std::__1::default_delete<std::__1::__thread_struct> >, augs::th
read_pool::make_continuous_worker()::{lambda()#1}>&, std::__1::__tuple_indices<>) (__t=...) at /usr/lib/llvm-14/bin/../include/c++/v1/thread:282
#17 std::__1::__thread_proxy<std::__1::tuple<std::__1::unique_ptr<std::__1::__thread_struct, std::__1::default_delete<std::__1::__thread_struct> >, augs::thread
_pool::make_continuous_worker()::{lambda()#1}> >(void*) (__vp=) at /usr/lib/llvm-14/bin/../include/c++/v1/thread:293
#18 ?? () from /usr/lib/libc.so.6
#19 ?? () from /usr/lib/libc.so.6


Program terminated with signal SIGSEGV, Segmentation fault.
#0  std::__1::__hash_table<std::__1::__hash_value_type<collision_cooldown_key, sound_system::collision_sound_succession>, std::__1::__unordered_map_hasher<colli
sion_cooldown_key, std::__1::__hash_value_type<collision_cooldown_key, sound_system::collision_sound_succession>, std::__1::hash<collision_cooldown_key>, std::_
_1::equal_to<collision_cooldown_key>, true>, std::__1::__unordered_map_equal<collision_cooldown_key, std::__1::__hash_value_type<collision_cooldown_key, sound_s
ystem::collision_sound_succession>, std::__1::equal_to<collision_cooldown_key>, std::__1::hash<collision_cooldown_key>, true>, std::__1::allocator<std::__1::__h
ash_value_type<collision_cooldown_key, sound_system::collision_sound_succession> > >::remove (this=this@entry=, __p=...) at /usr/lib/llvm-14/bin/../include/c++/
v1/__hash_table:2555
2555    /usr/lib/llvm-14/bin/../include/c++/v1/__hash_table: No such file or directory.
[Current thread is 1 (Thread 0x7f6083fef6c0 (LWP 33985))]
#0  std::__1::__hash_table<std::__1::__hash_value_type<collision_cooldown_key, sound_system::collision_sound_succession>, std::__1::__unordered_map_hasher<colli
sion_cooldown_key, std::__1::__hash_value_type<collision_cooldown_key, sound_system::collision_sound_succession>, std::__1::hash<collision_cooldown_key>, std::_
_1::equal_to<collision_cooldown_key>, true>, std::__1::__unordered_map_equal<collision_cooldown_key, std::__1::__hash_value_type<collision_cooldown_key, sound_s
ystem::collision_sound_succession>, std::__1::equal_to<collision_cooldown_key>, std::__1::hash<collision_cooldown_key>, true>, std::__1::allocator<std::__1::__h
ash_value_type<collision_cooldown_key, sound_system::collision_sound_succession> > >::remove (this=this@entry=, __p=...) at /usr/lib/llvm-14/bin/../include/c++/
v1/__hash_table:2555
#1  std::__1::__hash_table<std::__1::__hash_value_type<collision_cooldown_key, sound_system::collision_sound_succession>, std::__1::__unordered_map_hasher<colli
sion_cooldown_key, std::__1::__hash_value_type<collision_cooldown_key, sound_system::collision_sound_succession>, std::__1::hash<collision_cooldown_key>, std::_
_1::equal_to<collision_cooldown_key>, true>, std::__1::__unordered_map_equal<collision_cooldown_key, std::__1::__hash_value_type<collision_cooldown_key, sound_s
ystem::collision_sound_succession>, std::__1::equal_to<collision_cooldown_key>, std::__1::hash<collision_cooldown_key>, true>, std::__1::allocator<std::__1::__h
ash_value_type<collision_cooldown_key, sound_system::collision_sound_succession> > >::erase (this=, __p=...) at /usr/lib/llvm-14/bin/../include/c++/v1/__hash_ta
ble:2486
#2  std::__1::unordered_map<collision_cooldown_key, sound_system::collision_sound_succession, std::__1::hash<collision_cooldown_key>, std::__1::equal_to<collisi
on_cooldown_key>, std::__1::allocator<std::__1::pair<collision_cooldown_key const, sound_system::collision_sound_succession> > >::erase (this=, __p=...) at /usr
/lib/llvm-14/bin/../include/c++/v1/unordered_map:1339
#3  std::__1::__libcpp_erase_if_container<std::__1::unordered_map<collision_cooldown_key, sound_system::collision_sound_succession, std::__1::hash<collision_coo
ldown_key>, std::__1::equal_to<collision_cooldown_key>, std::__1::allocator<std::__1::pair<collision_cooldown_key const, sound_system::collision_sound_successio
n> > >, sound_system::fade_sources(augs::audio_renderer const&, augs::delta)::$_17>(std::__1::unordered_map<collision_cooldown_key, sound_system::collision_soun
d_succession, std::__1::hash<collision_cooldown_key>, std::__1::equal_to<collision_cooldown_key>, std::__1::allocator<std::__1::pair<collision_cooldown_key cons
t, sound_system::collision_sound_succession> > >&, sound_system::fade_sources(augs::audio_renderer const&, augs::delta)::$_17&) (__c=..., __pred=...) at /usr/li
b/llvm-14/bin/../include/c++/v1/__iterator/erase_if_container.h:30
#4  std::__1::erase_if<collision_cooldown_key, sound_system::collision_sound_succession, std::__1::hash<collision_cooldown_key>, std::__1::equal_to<collision_co
oldown_key>, std::__1::allocator<std::__1::pair<collision_cooldown_key const, sound_system::collision_sound_succession> >, sound_system::fade_sources(augs::audi
o_renderer const&, augs::delta)::$_17>(std::__1::unordered_map<collision_cooldown_key, sound_system::collision_sound_succession, std::__1::hash<collision_cooldo
wn_key>, std::__1::equal_to<collision_cooldown_key>, std::__1::allocator<std::__1::pair<collision_cooldown_key const, sound_system::collision_sound_succession> 
> >&, sound_system::fade_sources(augs::audio_renderer const&, augs::delta)::$_17) (__c=..., __pred=...) at /usr/lib/llvm-14/bin/../include/c++/v1/unordered_map:
1869
#5  sound_system::fade_sources (this=, renderer=..., dt=...) at /home/runner/work/Hypersomnia/Hypersomnia/src/view/audiovisual_state/systems/sound_system.cpp:97
6
#6  audiovisual_state::advance(audiovisual_advance_input)::$_13::operator()() const (this=) at /home/runner/work/Hypersomnia/Hypersomnia/src/view/audiovisual_st
ate/audiovisual_state.cpp:308
#7  audiovisual_state::advance(audiovisual_advance_input)::$_14::operator()() const (this=) at /home/runner/work/Hypersomnia/Hypersomnia/src/view/audiovisual_st
ate/audiovisual_state.cpp:317
#8  std::__1::__invoke<audiovisual_state::advance(audiovisual_advance_input)::$_14&>(audiovisual_state::advance(audiovisual_advance_input)::$_14&) (__f=...) at 
/usr/lib/llvm-14/bin/../include/c++/v1/type_traits:3640
#9  std::__1::__invoke_void_return_wrapper<void, true>::__call<audiovisual_state::advance(audiovisual_advance_input)::$_14&>(audiovisual_state::advance(audiovis
ual_advance_input)::$_14&) (__args=...) at /usr/lib/llvm-14/bin/../include/c++/v1/__functional/invoke.h:61
#10 std::__1::__function::__alloc_func<audiovisual_state::advance(audiovisual_advance_input)::$_14, std::__1::allocator<audiovisual_state::advance(audiovisual_a
dvance_input)::$_14>, void ()>::operator()() (this=) at /usr/lib/llvm-14/bin/../include/c++/v1/__functional/function.h:180
#11 std::__1::__function::__func<audiovisual_state::advance(audiovisual_advance_input)::$_14, std::__1::allocator<audiovisual_state::advance(audiovisual_advance
_input)::$_14>, void ()>::operator()() (this=) at /usr/lib/llvm-14/bin/../include/c++/v1/__functional/function.h:354
#12 std::__1::__function::__value_func<void ()>::operator()() const (this=) at /usr/lib/llvm-14/bin/../include/c++/v1/__functional/function.h:507
#13 std::__1::function<void ()>::operator()() const (this=) at /usr/lib/llvm-14/bin/../include/c++/v1/__functional/function.h:1184
#14 augs::thread_pool::make_continuous_worker()::{lambda()#1}::operator()() const (this=this@entry=) at /home/runner/work/Hypersomnia/Hypersomnia/src/augs/templ
ates/thread_pool.h:62
#15 std::__1::__invoke<augs::thread_pool::make_continuous_worker()::{lambda()#1}>(augs::thread_pool::make_continuous_worker()::{lambda()#1}&&) (__f=...) at /usr
/lib/llvm-14/bin/../include/c++/v1/type_traits:3640
#16 std::__1::__thread_execute<std::__1::unique_ptr<std::__1::__thread_struct, std::__1::default_delete<std::__1::__thread_struct> >, augs::thread_pool::make_co
ntinuous_worker()::{lambda()#1}>(std::__1::tuple<std::__1::unique_ptr<std::__1::__thread_struct, std::__1::default_delete<std::__1::__thread_struct> >, augs::th
read_pool::make_continuous_worker()::{lambda()#1}>&, std::__1::__tuple_indices<>) (__t=...) at /usr/lib/llvm-14/bin/../include/c++/v1/thread:282
#17 std::__1::__thread_proxy<std::__1::tuple<std::__1::unique_ptr<std::__1::__thread_struct, std::__1::default_delete<std::__1::__thread_struct> >, augs::thread
_pool::make_continuous_worker()::{lambda()#1}> >(void*) (__vp=) at /usr/lib/llvm-14/bin/../include/c++/v1/thread:293
#18 ?? () from /usr/lib/libc.so.6
#19 ?? () from /usr/lib/libc.so.6
