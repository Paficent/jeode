#include "core/log.h"
#include "core/overlay.h"

#include <mutex>

#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

template <typename Mutex> class overlay_sink : public spdlog::sinks::base_sink<Mutex> {
  protected:
	void sink_it_(const spdlog::details::log_msg &msg) override {
		spdlog::memory_buf_t buf;
		spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, buf);
		std::string formatted(buf.data(), buf.size());
		while (!formatted.empty() && (formatted.back() == '\n' || formatted.back() == '\r')) formatted.pop_back();
		overlay_log(formatted);
	}

	void flush_() override {}
};

using overlay_sink_mt = overlay_sink<std::mutex>;

void log_init(bool debug) {
	std::vector<spdlog::sink_ptr> sinks;

	auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("jeode/latest.log", true);
	file_sink->set_pattern("[%H:%M:%S.%e] [%l] %v");
	sinks.push_back(file_sink);

	auto imgui_sink = std::make_shared<overlay_sink_mt>();
	imgui_sink->set_pattern("[%l] [%H:%M:%S] %v");
	sinks.push_back(imgui_sink);

	auto logger = std::make_shared<spdlog::logger>("jeode", sinks.begin(), sinks.end());
	logger->set_level(debug ? spdlog::level::debug : spdlog::level::info);
	logger->flush_on(debug ? spdlog::level::debug : spdlog::level::info);

	spdlog::set_default_logger(logger);
}

void log_shutdown() {
	spdlog::shutdown();
}
