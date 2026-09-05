import std;
import meta;
import log;
import Http;

auto main() -> int {
    Log::SetLevel(LogLevel::Debug);

    const HttpClient client;
    const auto response = client.get("https://example.com");

    if (response.ok()) {
        Log::Info("GET example.com -> {} ({} bytes)", response.status_code, response.body.size());
    } else if (!response.error.empty()) {
        Log::Error("GET failed (curl): {}", response.error);
    } else {
        Log::Error("GET failed (http): {}", response.status_code);
    }

    std::println("Meta Information:");
    std::println("Version: {}", Meta::version);
    std::println("Author: {}", Meta::author);
    std::println("Description: {}", Meta::description);
    std::println("License: {}", Meta::license);
    std::println("Verify Link: {}", Meta::verify_link);
}
