export module http.curl.global;

export class CurlGlobal {
public:
    static auto ensure_initialized() -> void;

    CurlGlobal(const CurlGlobal &) = delete;
    CurlGlobal &operator=(const CurlGlobal &) = delete;

private:
    CurlGlobal();
    ~CurlGlobal();
};
