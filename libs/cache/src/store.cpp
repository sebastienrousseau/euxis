#include "euxis/cache/store.hpp"

#include <sqlite3.h>

#include <chrono>
#include <string>
#include <system_error>

namespace euxis::cache {

// ---- PIMPL state -----------------------------------------------------------

struct ScanCache::Impl {
    sqlite3* db{nullptr};
    std::filesystem::path db_path;

    ~Impl() {
        if (db != nullptr) sqlite3_close(db);
    }
};

ScanCache::ScanCache() : impl_(std::make_unique<Impl>()) {}
ScanCache::~ScanCache() = default;
ScanCache::ScanCache(ScanCache&&) noexcept            = default;
ScanCache& ScanCache::operator=(ScanCache&&) noexcept = default;

auto ScanCache::path() const noexcept -> const std::filesystem::path& {
    return impl_->db_path;
}

// ---- helpers ---------------------------------------------------------------

namespace {

auto make_error(sqlite3* db, std::string ctx) -> StoreError {
    return StoreError{
        .message = std::move(ctx) + ": " + (db != nullptr ? sqlite3_errmsg(db) : "no db"),
        .sqlite_code = db != nullptr ? sqlite3_extended_errcode(db) : 0,
    };
}

auto unix_now() -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

constexpr const char* kSchemaSql = R"SQL(
    CREATE TABLE IF NOT EXISTS scan_cache (
        cache_key     TEXT    PRIMARY KEY,
        file_path     TEXT    NOT NULL,
        content_hash  TEXT    NOT NULL,
        ruleset_hash  TEXT    NOT NULL,
        tool_version  TEXT    NOT NULL,
        findings_json BLOB,
        created_at    INTEGER NOT NULL,
        size_bytes    INTEGER NOT NULL
    );
    CREATE INDEX IF NOT EXISTS idx_created_at ON scan_cache(created_at);
)SQL";

} // namespace

// ---- open ------------------------------------------------------------------

auto ScanCache::open(const std::filesystem::path& path)
    -> std::expected<ScanCache, StoreError> {
    ScanCache cache;
    cache.impl_->db_path = path;

    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            return std::unexpected(StoreError{
                .message = "create_directories: " + ec.message(),
            });
        }
    }

    int rc = sqlite3_open_v2(
        path.string().c_str(),
        &cache.impl_->db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr);
    if (rc != SQLITE_OK) {
        return std::unexpected(make_error(cache.impl_->db, "open"));
    }

    // WAL gives single-writer/many-reader semantics across processes
    // and substantially reduces fsync cost on commits — the cache is
    // a high-frequency write target during scans.
    char* errmsg = nullptr;
    constexpr const char* kPragmas =
        "PRAGMA journal_mode=WAL;"
        "PRAGMA synchronous=NORMAL;"
        "PRAGMA temp_store=MEMORY;";
    rc = sqlite3_exec(cache.impl_->db, kPragmas, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        StoreError e{
            .message = std::string{"pragmas: "} + (errmsg != nullptr ? errmsg : "?"),
            .sqlite_code = rc,
        };
        sqlite3_free(errmsg);
        return std::unexpected(std::move(e));
    }

    rc = sqlite3_exec(cache.impl_->db, kSchemaSql, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        StoreError e{
            .message = std::string{"schema: "} + (errmsg != nullptr ? errmsg : "?"),
            .sqlite_code = rc,
        };
        sqlite3_free(errmsg);
        return std::unexpected(std::move(e));
    }
    return cache;
}

// ---- get -------------------------------------------------------------------

auto ScanCache::get(const std::string& cache_key) const
    -> std::expected<std::optional<CacheEntry>, StoreError> {
    if (impl_->db == nullptr) {
        return std::unexpected(StoreError{.message = "ScanCache not open"});
    }
    sqlite3_stmt* stmt = nullptr;
    constexpr const char* kSql =
        "SELECT findings_json, created_at, size_bytes "
        "  FROM scan_cache WHERE cache_key = ?1";
    if (sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::unexpected(make_error(impl_->db, "prepare get"));
    }
    sqlite3_bind_text(stmt, 1, cache_key.c_str(),
                      static_cast<int>(cache_key.size()), SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    std::optional<CacheEntry> out;
    if (rc == SQLITE_ROW) {
        const auto* blob = sqlite3_column_blob(stmt, 0);
        auto n           = sqlite3_column_bytes(stmt, 0);
        CacheEntry e;
        if (blob != nullptr && n > 0) {
            e.findings_json.assign(static_cast<const char*>(blob),
                                   static_cast<std::size_t>(n));
        }
        e.created_at_unix = sqlite3_column_int64(stmt, 1);
        e.size_bytes      = sqlite3_column_int64(stmt, 2);
        out = std::move(e);
    } else if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return std::unexpected(make_error(impl_->db, "step get"));
    }
    sqlite3_finalize(stmt);
    return out;
}

// ---- put -------------------------------------------------------------------

auto ScanCache::put(const KeyInputs& inputs, const CacheEntry& entry)
    -> std::expected<void, StoreError> {
    if (impl_->db == nullptr) {
        return std::unexpected(StoreError{.message = "ScanCache not open"});
    }
    auto key = compose_key(inputs);

    sqlite3_stmt* stmt = nullptr;
    constexpr const char* kSql =
        "INSERT OR REPLACE INTO scan_cache "
        "  (cache_key, file_path, content_hash, ruleset_hash, tool_version, "
        "   findings_json, created_at, size_bytes) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)";
    if (sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::unexpected(make_error(impl_->db, "prepare put"));
    }

    sqlite3_bind_text(stmt, 1, key.c_str(),
                      static_cast<int>(key.size()),         SQLITE_TRANSIENT);
    auto path_str = inputs.file.string();
    sqlite3_bind_text(stmt, 2, path_str.c_str(),
                      static_cast<int>(path_str.size()),    SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, inputs.content_hash.c_str(),
                      static_cast<int>(inputs.content_hash.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, inputs.ruleset_hash.c_str(),
                      static_cast<int>(inputs.ruleset_hash.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, inputs.tool_version.data(),
                      static_cast<int>(inputs.tool_version.size()), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 6, entry.findings_json.data(),
                      static_cast<int>(entry.findings_json.size()), SQLITE_TRANSIENT);
    std::int64_t created =
        entry.created_at_unix > 0 ? entry.created_at_unix : unix_now();
    sqlite3_bind_int64(stmt, 7, created);
    std::int64_t size = entry.size_bytes > 0
        ? entry.size_bytes
        : static_cast<std::int64_t>(entry.findings_json.size());
    sqlite3_bind_int64(stmt, 8, size);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return std::unexpected(make_error(impl_->db, "step put"));
    }
    return {};
}

// ---- purge -----------------------------------------------------------------

auto ScanCache::purge() -> std::expected<std::int64_t, StoreError> {
    if (impl_->db == nullptr) {
        return std::unexpected(StoreError{.message = "ScanCache not open"});
    }
    char* errmsg = nullptr;
    int rc = sqlite3_exec(impl_->db, "DELETE FROM scan_cache",
                          nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        StoreError e{
            .message = std::string{"purge: "} + (errmsg != nullptr ? errmsg : "?"),
            .sqlite_code = rc,
        };
        sqlite3_free(errmsg);
        return std::unexpected(std::move(e));
    }
    return sqlite3_changes(impl_->db);
}

auto ScanCache::purge_older_than(std::int64_t ttl_seconds,
                                  std::int64_t now_unix)
    -> std::expected<std::int64_t, StoreError> {
    if (impl_->db == nullptr) {
        return std::unexpected(StoreError{.message = "ScanCache not open"});
    }
    sqlite3_stmt* stmt = nullptr;
    constexpr const char* kSql =
        "DELETE FROM scan_cache WHERE created_at < ?1";
    if (sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::unexpected(make_error(impl_->db, "prepare purge_older"));
    }
    sqlite3_bind_int64(stmt, 1, now_unix - ttl_seconds);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return std::unexpected(make_error(impl_->db, "step purge_older"));
    }
    return sqlite3_changes(impl_->db);
}

// ---- stats -----------------------------------------------------------------

auto ScanCache::stats() const
    -> std::expected<CacheStats, StoreError> {
    if (impl_->db == nullptr) {
        return std::unexpected(StoreError{.message = "ScanCache not open"});
    }
    CacheStats out{};
    sqlite3_stmt* stmt = nullptr;
    constexpr const char* kSql =
        "SELECT COUNT(*), COALESCE(SUM(size_bytes), 0), "
        "       COALESCE(MIN(created_at), 0), COALESCE(MAX(created_at), 0) "
        "  FROM scan_cache";
    if (sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::unexpected(make_error(impl_->db, "prepare stats"));
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out.entry_count       = sqlite3_column_int64(stmt, 0);
        out.total_size_bytes  = sqlite3_column_int64(stmt, 1);
        out.oldest_entry_unix = sqlite3_column_int64(stmt, 2);
        out.newest_entry_unix = sqlite3_column_int64(stmt, 3);
    }
    sqlite3_finalize(stmt);
    return out;
}

} // namespace euxis::cache
