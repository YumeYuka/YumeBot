import std;
import config;
import daemon;
import log;
import meta;

auto main() -> int {
    Log::SetLevel(LogLevel::Debug);

    const auto config = Config::form_file("config.conf");
    Log::Info("Config loaded");
    Log::Info("{}", config.to_string());
    Log::Info("{} v{}", Meta::description, Meta::version);

    Daemon::run(config);
    return 0;
}
