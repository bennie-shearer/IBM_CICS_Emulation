#pragma once
#include "cics/common/types.hpp"
#include "cics/common/error.hpp"

namespace cics::gdg {

enum class GdgModel : UInt8 { FIFO = 1, LIFO = 2 };

struct GdgBase {
    String name;
    UInt16 limit = 255;
    GdgModel model = GdgModel::FIFO;
    bool scratch = true;
    bool empty = false;
    bool purge = false;
    SystemTimePoint created;
    String owner;
};

struct GdgGeneration {
    String base_name;
    String generation_name;  // e.g., BASE.G0001V00
    Int16 relative_number;   // +1, 0, -1, -2...
    UInt16 absolute_number;  // G0001, G0002...
    UInt8 version = 0;
    SystemTimePoint created;
    UInt64 size_bytes = 0;
    String volume;
    bool active = true;
};

[[nodiscard]] constexpr StringView to_string(GdgModel model) {
    switch (model) {
        case GdgModel::FIFO: return "FIFO";
        case GdgModel::LIFO: return "LIFO";
    }
    return "UNKNOWN";
}

[[nodiscard]] String generate_generation_name(const String& base_name, UInt16 gen_number, UInt8 version = 0);
[[nodiscard]] Result<std::pair<UInt16, UInt8>> parse_generation_name(const String& gen_name);

class GdgManager {
private:
    std::unordered_map<String, GdgBase> bases_;
    std::unordered_map<String, std::vector<GdgGeneration>> generations_;
    mutable std::shared_mutex mutex_;
    
public:
    Result<void> define_base(const GdgBase& base);
    Result<void> delete_base(const String& name);
    Result<GdgBase> get_base(const String& name);
    
    Result<GdgGeneration> create_generation(const String& base_name);
    Result<GdgGeneration> get_generation(const String& base_name, Int16 relative);
    Result<void> roll_off(const String& base_name);
    
    std::vector<GdgGeneration> list_generations(const String& base_name);
    [[nodiscard]] Size generation_count(const String& base_name) const;
};

} // namespace cics::gdg
