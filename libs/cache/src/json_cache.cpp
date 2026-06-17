#include "euxis/cache/json_cache.hpp"

#include "euxis/cache/hash.hpp"

#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

#include <sqlite3.h>

namespace euxis::cache {

struct JsonCache::Impl {
    sqlite3* db{nullptr};
    std::filesystem::path db_path;
    mutable JsonCacheStats stats;

    ~Impl() {
        if (db != nullptr) sqlite3_close(db);
    }
};

JsonCache::JsonCache() : impl_(std::make_unique<Impl>()) {}
JsonCache::~JsonCache() = default;
JsonCache::JsonCache(JsonCache&&) noexcept            = default;
JsonCache& JsonCache::operator=(JsonCache&&) noexcept = default;

auto JsonCache::path() const noexcept -> const std::filesystem::path& {
    return impl_->db_path;
}

auto JsonCache::stats() const noexcept -> JsonCacheStats {
    return impl_->stats;
}

namespace {

auto make_error(sqlite3* db, std::string ctx) -> JsonCacheError {
    return JsonCacheError{
        .message = std::move(ctx) + ": " +
                   (db != nullptr ? sqlite3_errmsg(db) : "no db"),
    };
}

constexpr const char* kSchemaSql = R"SQL(
    CREATE TABLE IF NOT EXISTS json_cache (
        file_path     TEXT    PRIMARY KEY,
        content_hash  TEXT    NOT NULL,
        msgpack_blob  BLOB    NOT NULL,
        stored_at     INTEGER NOT NULL,
        size_bytes    INTEGER NOT NULL
    );
    CREATE INDEX IF NOT EXISTS idx_json_stored_at ON json_cache(stored_at);
)SQL";

auto current_unix() noexcept -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace

auto JsonCache::open(const std::filesystem::path& db_path)
    -> std::expected<JsonCache, JsonCacheError> {
    JsonCache cache;
    cache.impl_->db_path = db_path;

    if (db_path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(db_path.parent_path(), ec);
        if (ec) {
            return std::unexpected(JsonCacheError{
                .message = "create_directories: " + ec.message(),
            });
        }
    }

    int rc = sqlite3_open_v2(
        db_path.string().c_str(),
        &cache.impl_->db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr);
    if (rc != SQLITE_OK) {
        return std::unexpected(make_error(cache.impl_->db, "open"));
    }

    char* errmsg = nullptr;
    constexpr const char* kPragmas =
        "PRAGMA journal_mode=WAL;"
        "PRAGMA synchronous=NORMAL;"
        "PRAGMA temp_store=MEMORY;";
    rc = sqlite3_exec(cache.impl_->db, kPragmas, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        JsonCacheError e{
            .message = std::string{"pragmas: "} + (errmsg != nullptr ? errmsg : "?"),
        };
        sqlite3_free(errmsg);
        return std::unexpected(std::move(e));
    }

    rc = sqlite3_exec(cache.impl_->db, kSchemaSql, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        JsonCacheError e{
            .message = std::string{"schema: "} + (errmsg != nullptr ? errmsg : "?"),
        };
        sqlite3_free(errmsg);
        return std::unexpected(std::move(e));
    }
    return cache;
}

auto JsonCache::get(const std::filesystem::path& file) const
    -> std::expected<std::optional<nlohmann::json>, JsonCacheError> {
    if (impl_->db == nullptr) {
        return std::unexpected(JsonCacheError{.message = "JsonCache not open"});
    }
    if (!std::filesystem::exists(file)) {
        return std::nullopt;
    }
    auto current_hash = hash_file(file);
    if (!current_hash) {
        return std::unexpected(JsonCacheError{
            .message = "hash_file: " + current_hash.error().message,
        });
    }

    sqlite3_stmt* stmt = nullptr;
    constexpr const char* kSql =
        "SELECT msgpack_blob, content_hash FROM json_cache WHERE file_path = ?1";
    if (sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::unexpected(make_error(impl_->db, "prepare get"));
    }
    auto path_str = file.string();
    sqlite3_bind_text(stmt, 1, path_str.c_str(),
                      static_cast<int>(path_str.size()), SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    std::optional<nlohmann::json> out;
    if (rc == SQLITE_ROW) {
        const auto* hash_bytes = sqlite3_column_text(stmt, 1);
        std::string stored_hash =
            hash_bytes != nullptr
                ? reinterpret_cast<const char*>(hash_bytes)
                : std::string{};
        if (stored_hash == *current_hash) {
            const auto* blob_ptr = sqlite3_column_blob(stmt, 0);
            auto blob_size       = sqlite3_column_bytes(stmt, 0);
            if (blob_ptr != nullptr && blob_size > 0) {
                try {
                    out = nlohmann::json::from_msgpack(
                        std::vector<std::uint8_t>(
                            static_cast<const std::uint8_t*>(blob_ptr),
                            static_cast<const std::uint8_t*>(blob_ptr) + blob_size));
                    ++impl_->stats.hits;
                } catch (const std::exception&) {
                    // Corrupt cache entry — treat as a miss.
                    out.reset();
                }
            }
        }
    } else if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return std::unexpected(make_error(impl_->db, "step get"));
    }
    sqlite3_finalize(stmt);
    if (!out.has_value()) {
        ++impl_->stats.misses;
    }
    return out;
}

auto JsonCache::put(const std::filesystem::path& file,
                    const nlohmann::json& parsed)
    -> std::expected<void, JsonCacheError> {
    if (impl_->db == nullptr) {
        return std::unexpected(JsonCacheError{.message = "JsonCache not open"});
    }
    auto current_hash = hash_file(file);
    if (!current_hash) {
        return std::unexpected(JsonCacheError{
            .message = "hash_file: " + current_hash.error().message,
        });
    }
    auto msgpack = nlohmann::json::to_msgpack(parsed);

    sqlite3_stmt* stmt = nullptr;
    constexpr const char* kSql =
        "INSERT OR REPLACE INTO json_cache "
        "  (file_path, content_hash, msgpack_blob, stored_at, size_bytes) "
        "VALUES (?1, ?2, ?3, ?4, ?5)";
    if (sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::unexpected(make_error(impl_->db, "prepare put"));
    }

    auto path_str = file.string();
    sqlite3_bind_text(stmt, 1, path_str.c_str(),
                      static_cast<int>(path_str.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, current_hash->c_str(),
                      static_cast<int>(current_hash->size()), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 3, msgpack.data(),
                      static_cast<int>(msgpack.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, current_unix());
    sqlite3_bind_int64(stmt, 5, static_cast<std::int64_t>(msgpack.size()));

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return std::unexpected(make_error(impl_->db, "step put"));
    }
    impl_->stats.bytes_stored += msgpack.size();
    ++impl_->stats.entries;
    return {};
}

auto JsonCache::get_or_load(const std::filesystem::path& file)
    -> std::expected<nlohmann::json, JsonCacheError> {
    auto cached = get(file);
    if (!cached) return std::unexpected(cached.error());
    if (cached->has_value()) return std::move(**cached);

    // Miss path: read the file and parse.
    std::ifstream f(file);
    if (!f.is_open()) {
        return std::unexpected(JsonCacheError{
            .message = "cannot open file: " + file.string(),
        });
    }
    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(f);
    } catch (const std::exception& e) {
        return std::unexpected(JsonCacheError{
            .message = std::string{"parse: "} + e.what(),
        });
    }
    auto stored = put(file, parsed);
    if (!stored) return std::unexpected(stored.error());
    return parsed;
}

auto JsonCache::purge() -> std::expected<std::size_t, JsonCacheError> {
    if (impl_->db == nullptr) {
        return std::unexpected(JsonCacheError{.message = "JsonCache not open"});
    }
    char* errmsg = nullptr;
    int rc = sqlite3_exec(impl_->db, "DELETE FROM json_cache",
                          nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        JsonCacheError e{
            .message = std::string{"purge: "} + (errmsg != nullptr ? errmsg : "?"),
        };
        sqlite3_free(errmsg);
        return std::unexpected(std::move(e));
    }
    auto removed = static_cast<std::size_t>(sqlite3_changes(impl_->db));
    impl_->stats = JsonCacheStats{};
    return removed;
}

} // namespace euxis::cache
