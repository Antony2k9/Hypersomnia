#include "augs/log.h"

#include <iostream>
#include <string>
#include <gtest/gtest.h>

#include "augs/math/vec2.h"

#include <fstream>
#include <thread>
#include <mutex>

#include "augs/templates/string_templates.h"

#define ENABLE_LOG 1
#define LOG_TO_FILE 1

std::mutex log_mutex;

unsigned global_log::max_entries = 40;
std::vector<log_entry> global_log::recent_entries;

augs::gui::text::fstr global_log::format_recent_as_text(const assets::font_id f) {
	augs::gui::text::fstr result;
	
	for (const auto& line : recent_entries) {
		auto wstr = to_wstring(line.text + "\n");
		result += augs::gui::text::format(wstr, augs::gui::text::style(f, augs::rgba(line.color)));
	}

	return result;
}

void global_log::push_entry(const log_entry new_entry) {
	recent_entries.push_back(new_entry);

	if (recent_entries.size() > max_entries)
		recent_entries.erase(recent_entries.begin());
}

template<>
void LOG(const std::string& f) {
#if ENABLE_LOG 
	std::unique_lock<std::mutex> lock(log_mutex);

	global_log::push_entry({ console_color::WHITE, f });

	std::cout << f << std::endl;
#if LOG_TO_FILE
	std::ofstream recording_file("live_debug.txt", std::ios::out | std::ios::app);
	recording_file << f << std::endl;
#endif
#endif
}

template<>
void LOG_COLOR(const console_color c, const std::string& f) {
#if ENABLE_LOG 
	std::unique_lock<std::mutex> lock(log_mutex);
	
	global_log::push_entry({ c, f });

	augs::colored_print(c, f.c_str());
#if LOG_TO_FILE
	std::ofstream recording_file("live_debug.txt", std::ios::out | std::ios::app);
	recording_file << f << std::endl;
#endif
#endif
}

void CALL_SHELL(const std::string& s) {
	std::unique_lock<std::mutex> lock(log_mutex);

	system(s.c_str());
}

TEST(TypesafeSprintf, TypesafeSprintfSeveralTests) {
	EXPECT_EQ("1,2,3:4", typesafe_sprintf("%x,%x,%x:%x", 1, 2, 3, 4));
	EXPECT_EQ("abc,2,3:def", typesafe_sprintf("%x,%x,%x:%x", "abc", 2, 3, "def"));
	EXPECT_EQ("abc,2.55,3.14:def", typesafe_sprintf("%x,%x,%x:%x", "abc", 2.55, 3.14f, "def"));

	vec2 test(123, 412);

	EXPECT_EQ("Vector is equal to: (123,412)", typesafe_sprintf("Vector is equal to: %x", test));

	int errid = 1282;
	std::string location = "augs::window::glwindow::create";

	EXPECT_EQ("OpenGL error 1282 in augs::window::glwindow::create", typesafe_sprintf("OpenGL error %x in %x", errid, location));
	
	int a = 2;
	LOG_NVPS("Test nvps: ", a, errid, test);
}