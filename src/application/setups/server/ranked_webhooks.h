#pragma once

namespace ranked_webhooks {
	template <class F>
	inline std::string json_report_match(
		const std::string& server_name,
		const std::string& arena,
		const std::string& game_mode,
		const messages::match_summary_message& info,
		F&& mode_id_to_account_id
	) {
		using namespace rapidjson;

		StringBuffer s;
		Writer<StringBuffer> writer(s);

		writer.StartObject();

		writer.Key("server_name");
		writer.String(server_name.c_str());

		writer.Key("arena");
		writer.String(arena.c_str());

		writer.Key("game_mode");
		writer.String(game_mode.c_str());

		writer.Key("win_score");
		writer.Int(info.first_team_score);

		writer.Key("lose_score");
		writer.Int(info.second_team_score);

		writer.Key("nicknames");

		{
			writer.StartObject();

			auto write_faction = [&](const auto& faction) {
				for (const auto& f : faction) {
					const auto id = mode_id_to_account_id(f.id);
					writer.Key(id.c_str());
					writer.String(f.nickname.c_str());
				}
			};

			write_faction(info.first_faction);
			write_faction(info.second_faction);

			writer.EndObject();
		}

		auto write_faction_ids = [&](const auto& faction) {
			writer.StartArray();

			for (const auto& f : faction) {
				const auto id = mode_id_to_account_id(f.id);
				writer.String(id.c_str());
			}

			writer.EndArray();
		};

		writer.Key("win_players");
		write_faction_ids(info.first_faction);

		writer.Key("lose_players");
		write_faction_ids(info.second_faction);

		writer.EndObject();

		return std::string(s.GetString());
	}
}

