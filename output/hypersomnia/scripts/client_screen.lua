
dofile "hypersomnia\\scripts\\protocol.lua"

dofile "hypersomnia\\scripts\\reliable_channel.lua"


dofile "hypersomnia\\scripts\\messages\\network_message.lua"
dofile "hypersomnia\\scripts\\messages\\server_commands.lua"

dofile "hypersomnia\\scripts\\components\\input_sync.lua"
dofile "hypersomnia\\scripts\\components\\input_prediction.lua"

dofile "hypersomnia\\scripts\\sync_modules\\modules.lua"
dofile "hypersomnia\\scripts\\sync_modules\\movement_sync.lua"

dofile "hypersomnia\\scripts\\systems\\client_system.lua"
dofile "hypersomnia\\scripts\\systems\\input_sync_system.lua"
dofile "hypersomnia\\scripts\\systems\\input_prediction_system.lua"
dofile "hypersomnia\\scripts\\systems\\synchronization_system.lua"

client_screen = inherits_from ()

function client_screen:constructor(camera_rect)
	self.sample_scene = scene_class:create()
	self.sample_scene:load_map("hypersomnia\\data\\maps\\sample_map.lua", "hypersomnia\\scripts\\loaders\\basic_map_loader.lua")
	
	self.sample_scene.world_camera.camera.screen_rect = camera_rect
	
	self.server = network_interface()
	
	self.server:connect(config_table.server_address, config_table.server_port)
	
	if config_table.simulate_lag ~= 0 then
		print "Simulating lag..."
		self.server:enable_lag(config_table.packet_loss, config_table.min_latency, config_table.jitter)
	end
	
	self.received = network_packet()
	
	-- entity system setup
	self.entity_system_instance = entity_system:create()
	
	self.entity_system_instance:register_messages {
		"network_message",
		"server_commands"
	}
	
	self.systems = {}
	self.systems.client = client_system:create(self.server)
	self.systems.input_sync = input_sync_system:create(self.sample_scene.world_object)
	self.systems.input_prediction = input_prediction_system:create(self.sample_scene.simulation_world, self.sample_scene.world_object)
	self.systems.synchronization = synchronization_system:create(self.sample_scene)
	
	self.entity_system_instance:register_systems(self.systems)
end

function client_screen:loop()

	setlsys(self.sample_scene.world_object.render_system)
		
	-- handle networking
	local packet = self.received
	
	if (self.server:receive(self.received)) then
		local message_type = self.received:byte(0)
			
		if message_type == network_event.ID_CONNECTION_REQUEST_ACCEPTED then
			self.server_guid = self.received:guid()
			self.systems.client.server_guid = self.server_guid
			print("Our connection request has been accepted.");
		elseif message_type == network_event.ID_NO_FREE_INCOMING_CONNECTIONS then
			print("The server is full.\n")
		elseif message_type == network_event.ID_DISCONNECTION_NOTIFICATION then
			print("Server has disconnected.\n")
		elseif message_type == network_event.ID_CONNECTION_LOST then
			print("Server lost the connection.\n")
		elseif message_type == protocol.GAME_TRANSMISSION then
			self.entity_system_instance:post(network_message:create {
				data = packet
			})
		end
	end
	
	
	
	
	self.systems.client:handle_incoming_commands()
	
	--print "sync upd"
	self.systems.synchronization:update()
	
	--print "input upd"
	self.systems.input_sync:update()
	
	self.systems.input_prediction:update()
	--print "client tick"
	self.systems.client:update_tick()
	
	--print "flush"
	self.entity_system_instance:flush_messages()

	--print "scene loop"
	self.sample_scene:loop()
end

function client_screen:close_connection()
	if self.server_guid ~= nil then
		self.server:close_connection(self.server_guid, send_priority.IMMEDIATE_PRIORITY)
	end
	self.server_guid = nil
end

