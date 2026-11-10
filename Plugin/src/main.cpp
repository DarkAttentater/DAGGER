// DAGGER (Dark's Adaptive Goam & GoamEER Ear Retractor): hides Goam's Elven Ears / Goam's Elven Ears Redone ear meshes on the player while a hood or
// helmet covers slot 30 (Head) / 31 (Hair). It flips the app-culled flag on the ear geometry, so there is
// no head-part swap and no head rebuild: the change is instant and leaves the character data untouched.
#include <atomic>
#include <chrono>
#include <fstream>
#include <thread>
#include <vector>

#include <spdlog/sinks/basic_file_sink.h>

namespace
{
	constexpr auto kPluginName = "DAGGER";

	struct Config
	{
		// The game names a head part's geometry after the head part's editor ID (seen in the log: "GoamLrgDark"),
		// not after the shape name inside the .nif. So ear geometry is matched by editor-ID prefix (case-insensitive).
		//   Goam's Elven Ears Redone: GoamSm*, GoamLrg*, GoamXL*  (00_GoamNoEars has no mesh)
		//   Goam's Elven Ears:        GoamElvenEars*
		std::vector<std::string> prefixes{ "GoamSm", "GoamLrg", "GoamXL", "GoamElvenEars" };
		std::vector<std::string> nodes;  // optional exact geometry names, in addition to the prefixes
		std::uint32_t            slotMask{ 3 };      // BipedObjectSlot bits: 1 = Head (30), 2 = Hair (31)
		bool                     ignoreCirclets{ true };  // skip armor with the ClothingCirclet keyword
		int                      pollMs{ 250 };      // idle re-check interval (catches model reloads)
		int                      fastPollMs{ 40 };   // re-check interval right after an equip/load
		std::string              logLevel{ "info" };
	};

	Config g_cfg;

	std::string Trim(std::string s)
	{
		const auto b = s.find_first_not_of(" \t\r\n");
		const auto e = s.find_last_not_of(" \t\r\n");
		return b == std::string::npos ? std::string{} : s.substr(b, e - b + 1);
	}

	std::string Lower(std::string s)
	{
		for (auto& c : s) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		return s;
	}

	void InitLog()
	{
		auto dir = SKSE::log::log_directory();
		if (!dir) {
			return;
		}
		try {
			auto path = *dir / (std::string(kPluginName) + ".log");
			auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
			auto log = std::make_shared<spdlog::logger>("dagger", std::move(sink));
			log->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
			log->set_level(spdlog::level::trace);
			log->flush_on(spdlog::level::info);
			spdlog::set_default_logger(std::move(log));
		} catch (...) {
		}
	}

	std::vector<std::string> ParseList(const std::string& a_val)
	{
		std::vector<std::string> v;
		std::size_t              start = 0;
		while (start <= a_val.size()) {
			const auto comma = a_val.find(',', start);
			const auto item = Trim(a_val.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
			if (!item.empty()) {
				v.push_back(item);
			}
			if (comma == std::string::npos) {
				break;
			}
			start = comma + 1;
		}
		return v;
	}

	void LoadConfig()
	{
		std::ifstream in("Data/SKSE/Plugins/DAGGER.ini");
		if (!in) {
			SKSE::log::info("No ini found, using defaults");
			return;
		}
		std::string line;
		while (std::getline(in, line)) {
			if (const auto c = line.find_first_of(";#"); c != std::string::npos) {
				line.erase(c);
			}
			const auto eq = line.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			const auto key = Lower(Trim(line.substr(0, eq)));
			const auto val = Trim(line.substr(eq + 1));
			try {
				if (key == "nodes") {
					g_cfg.nodes = ParseList(val);
				} else if (key == "prefixes") {
					if (auto v = ParseList(val); !v.empty()) {
						g_cfg.prefixes = std::move(v);
					}
				} else if (key == "slotmask") {
					g_cfg.slotMask = static_cast<std::uint32_t>(std::stoul(val, nullptr, 0));
				} else if (key == "ignorecirclets") {
					const auto l = Lower(val);
					g_cfg.ignoreCirclets = (l == "1" || l == "true" || l == "yes" || l == "on");
				} else if (key == "pollms") {
					g_cfg.pollMs = std::clamp(std::stoi(val), 50, 5000);
				} else if (key == "fastpollms") {
					g_cfg.fastPollMs = std::clamp(std::stoi(val), 10, 1000);
				} else if (key == "loglevel") {
					g_cfg.logLevel = Lower(val);
				}
			} catch (...) {
				SKSE::log::warn("Ignoring bad ini value for '{}'", key);
			}
		}
	}

	void ApplyLogLevel()
	{
		spdlog::level::level_enum level = spdlog::level::info;
		if (g_cfg.logLevel == "trace") {
			level = spdlog::level::trace;
		} else if (g_cfg.logLevel == "debug") {
			level = spdlog::level::debug;
		} else if (g_cfg.logLevel == "warn") {
			level = spdlog::level::warn;
		} else if (g_cfg.logLevel == "error") {
			level = spdlog::level::err;
		}
		spdlog::set_level(level);
	}

	// ---- game logic (main thread only) -------------------------------------------------------------

	bool IsCovered(RE::Actor* a_actor)
	{
		for (std::uint32_t bit = 0; bit < 32; ++bit) {
			const std::uint32_t mask = 1u << bit;
			if ((g_cfg.slotMask & mask) == 0) {
				continue;
			}
			const auto armor = a_actor->GetWornArmor(static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(mask));
			if (!armor) {
				continue;
			}
			// Circlets normally sit in their own slot (42) and never reach this point; this also skips
			// circlets from mods that put them in slot 30/31.
			if (g_cfg.ignoreCirclets && armor->HasKeywordString("ClothingCirclet"sv)) {
				continue;
			}
			return true;
		}
		return false;
	}

	void ListGeometry(RE::NiAVObject* a_obj, int a_depth, std::string& a_out)
	{
		if (!a_obj || a_depth > 12) {
			return;
		}
		if (a_obj->AsGeometry()) {
			a_out += std::string(a_obj->name.c_str() ? a_obj->name.c_str() : "?") + " | ";
		}
		if (const auto node = a_obj->AsNode()) {
			for (auto& child : node->GetChildren()) {
				ListGeometry(child.get(), a_depth + 1, a_out);
			}
		}
	}

	bool IsEarGeometry(const char* a_name)
	{
		if (!a_name || !*a_name) {
			return false;
		}
		const auto name = Lower(a_name);
		for (const auto& p : g_cfg.prefixes) {
			if (name.starts_with(Lower(p))) {
				return true;
			}
		}
		for (const auto& n : g_cfg.nodes) {
			if (name == Lower(n)) {
				return true;
			}
		}
		return false;
	}

	// Sets the culled state of every ear geometry under a_obj. Returns how many were found.
	std::size_t CullEars(RE::NiAVObject* a_obj, int a_depth, bool a_cull, std::string& a_names)
	{
		if (!a_obj || a_depth > 12) {
			return 0;
		}
		std::size_t found = 0;
		if (a_obj->AsGeometry() && IsEarGeometry(a_obj->name.c_str())) {
			++found;
			a_names += std::string(a_obj->name.c_str()) + " ";
			if (a_obj->GetAppCulled() != a_cull) {
				a_obj->SetAppCulled(a_cull);
			}
		}
		if (const auto node = a_obj->AsNode()) {
			for (auto& child : node->GetChildren()) {
				found += CullEars(child.get(), a_depth + 1, a_cull, a_names);
			}
		}
		return found;
	}

	// Logs a status line only when it differs from the last one, so the log stays short.
	void LogOnChange(const std::string& a_msg, std::size_t a_slot = 0)
	{
		static std::array<std::string, 2> s_last;
		if (a_slot < s_last.size() && a_msg != s_last[a_slot]) {
			s_last[a_slot] = a_msg;
			SKSE::log::info("{}", a_msg);
		}
	}

	void Apply()
	{
		const auto pc = RE::PlayerCharacter::GetSingleton();
		if (!pc || !pc->Is3DLoaded()) {
			LogOnChange("Apply: player 3D not loaded");
			return;
		}
		if (const auto ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) {
			LogOnChange("Apply: loading menu open");
			return;
		}
		const auto root = pc->Get3D();
		if (!root) {
			LogOnChange("Apply: no player 3D root");
			return;
		}

		static bool s_lastCovered = false;
		static bool s_dumped = false;

		const bool covered = IsCovered(pc);
		LogOnChange(std::string("Apply: running, head ") + (covered ? "covered" : "uncovered"));
		if (covered != s_lastCovered) {
			SKSE::log::info("Head {}", covered ? "covered: hiding ears" : "uncovered: showing ears");
			s_lastCovered = covered;
		}

		std::string matched;
		const auto  found = CullEars(root, 0, covered, matched);
		LogOnChange(std::string("Ear geometry: ") + std::to_string(found) + " matched (" + matched + ")" + (covered ? ", culled" : ", shown"), 1);

		// If nothing matched while covered, list the player's geometry once so the prefixes can be fixed in the ini.
		if (covered && found == 0 && !s_dumped) {
			s_dumped = true;
			std::string names;
			ListGeometry(root, 0, names);
			SKSE::log::warn("No ear geometry matched the configured prefixes/names. Geometry present: {}", names);
		}
	}

	// ---- scheduling --------------------------------------------------------------------------------

	std::atomic<std::int64_t> g_fastUntilMs{ 0 };
	std::atomic<bool>         g_taskPending{ false };

	std::int64_t NowMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	void BoostPolling()
	{
		g_fastUntilMs.store(NowMs() + 2000);
	}

	void PollLoop()
	{
		const auto tasks = SKSE::GetTaskInterface();
		if (!tasks) {
			SKSE::log::error("Task interface unavailable; plugin stays idle");
			return;
		}
		for (;;) {
			const bool fast = NowMs() < g_fastUntilMs.load();
			std::this_thread::sleep_for(std::chrono::milliseconds(fast ? g_cfg.fastPollMs : g_cfg.pollMs));
			if (g_taskPending.exchange(true)) {
				continue;  // previous pass has not run yet (loading screen, paused)
			}
			tasks->AddTask([] {
				try {
					Apply();
				} catch (const std::exception& e) {
					SKSE::log::error("Apply failed: {}", e.what());
				} catch (...) {
					SKSE::log::error("Apply failed");
				}
				g_taskPending.store(false);
			});
		}
	}

	class EquipSink : public RE::BSTEventSink<RE::TESEquipEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
		{
			if (a_event && a_event->actor && a_event->actor->IsPlayerRef()) {
				BoostPolling();
				if (const auto armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(a_event->baseObject)) {
					const auto name = armor->GetName();
					SKSE::log::info("Player {} armor '{}' [{:08X}] slots 0x{:X}{}", a_event->equipped ? "equipped" : "unequipped",
						name ? name : "", armor->GetFormID(), armor->GetSlotMask().underlying(),
						armor->HasKeywordString("ClothingCirclet"sv) ? " (ClothingCirclet)" : "");
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	EquipSink g_equipSink;

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		try {
			switch (a_msg->type) {
			case SKSE::MessagingInterface::kDataLoaded:
				if (const auto holder = RE::ScriptEventSourceHolder::GetSingleton()) {
					holder->AddEventSink<RE::TESEquipEvent>(&g_equipSink);
				}
				BoostPolling();
				std::thread(PollLoop).detach();
				SKSE::log::info("Ready: watching {} ear node name(s), slot mask 0x{:X}", g_cfg.nodes.size(), g_cfg.slotMask);
				break;
			case SKSE::MessagingInterface::kPostLoadGame:
			case SKSE::MessagingInterface::kNewGame:
				BoostPolling();
				break;
			default:
				break;
			}
		} catch (...) {
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	InitLog();
	SKSE::log::info("{} {}.{}.{} r{} starting (runtime {})", kPluginName, DAGGER_VER_MAJOR, DAGGER_VER_MINOR, DAGGER_VER_PATCH, DAGGER_REVISION, a_skse->RuntimeVersion().string());
	LoadConfig();
	ApplyLogLevel();

	if (const auto messaging = SKSE::GetMessagingInterface()) {
		messaging->RegisterListener(OnMessage);
	} else {
		SKSE::log::error("Messaging interface unavailable; plugin stays idle");
	}
	return true;
}

// Address Library (no hard-coded offsets), post-1.6.629 struct layouts as provided by CommonLibSSE-NG.
SKSEPluginInfo(
	.Version = REL::Version{ DAGGER_VER_MAJOR, DAGGER_VER_MINOR, DAGGER_VER_PATCH, DAGGER_REVISION },
	.Name = "DAGGER"sv,
	.Author = "DarkAttentater"sv,
	.StructCompatibility = SKSE::StructCompatibility::Dependent,
	.RuntimeCompatibility = SKSE::PluginDeclaration::RuntimeCompatibility(SKSE::VersionIndependence::AddressLibrary, true),
	.MinimumSKSEVersion = REL::Version{ 0, 0, 0, 0 })
