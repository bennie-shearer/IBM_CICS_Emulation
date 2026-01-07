#pragma once
#include "cics/common/types.hpp"
#include "cics/common/error.hpp"
#include <deque>

namespace cics::gdg {

enum class GdgModel : UInt8 { FIFO = 1, LIFO = 2 };

struct Generation {
    String absolute_name; Int32 relative_number; SystemTimePoint created;
    UInt64 size_bytes = 0; bool active = true;
    [[nodiscard]] String format_relative() const { return std::format("({})", relative_number); }
};

struct GdgBase {
    String base_name; UInt16 limit = 255; GdgModel model = GdgModel::FIFO;
    bool scratch = true; bool empty = false; bool extended = false;
    std::deque<Generation> generations; SystemTimePoint created;
    
    [[nodiscard]] Size generation_count() const { return generations.size(); }
    [[nodiscard]] bool is_full() const { return generations.size() >= limit; }
    [[nodiscard]] Optional<Generation> current() const { return generations.empty() ? std::nullopt : Optional<Generation>(generations.back()); }
};

class GdgManager {
    std::unordered_map<String, GdgBase> bases_;
    mutable std::shared_mutex mutex_;
public:
    static GdgManager& instance();
    
    Result<void> define_base(GdgBase base);
    Result<void> delete_base(const String& name);
    Result<GdgBase*> get_base(const String& name);
    Result<Generation> create_generation(const String& base_name);
    Result<Generation> get_generation(const String& base_name, Int32 relative);
    Result<void> roll_off(const String& base_name);
    [[nodiscard]] std::vector<String> list_bases() const;
};

[[nodiscard]] String format_generation_name(const String& base, Int32 gen_number);
[[nodiscard]] Result<std::pair<String, Int32>> parse_generation_name(const String& name);

} // namespace cics::gdg
