export module daemon;

import telegram;
import config;
import log;
import std;

export  class Daemon {
public:
    static auto run(const Config &config) -> void;
};
