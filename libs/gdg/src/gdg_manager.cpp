#include "cics/gdg/gdg_types.hpp"

namespace cics::gdg {

String generate_generation_name(const String& base_name, UInt16 gen_number, UInt8 version) {
    return std::format("{}.G{:04d}V{:02d}", base_name, gen_number, version);
}

Result<std::pair<UInt16, UInt8>> parse_generation_name(const String& gen_name) {
    auto dot_pos = gen_name.rfind('.');
    if (dot_pos == String::npos || dot_pos + 8 > gen_name.size()) {
        return make_error<std::pair<UInt16, UInt8>>(ErrorCode::INVALID_ARGUMENT, "Invalid generation name");
    }
    
    String suffix = gen_name.substr(dot_pos + 1);
    if (suffix.size() != 8 || suffix[0] != 'G' || suffix[5] != 'V') {
        return make_error<std::pair<UInt16, UInt8>>(ErrorCode::INVALID_ARGUMENT, "Invalid format");
    }
    
    try {
        UInt16 gen = static_cast<UInt16>(std::stoi(suffix.substr(1, 4)));
        UInt8 ver = static_cast<UInt8>(std::stoi(suffix.substr(6, 2)));
        return make_success(std::make_pair(gen, ver));
    } catch (...) {
        return make_error<std::pair<UInt16, UInt8>>(ErrorCode::INVALID_ARGUMENT, "Parse error");
    }
}

Result<void> GdgManager::define_base(const GdgBase& base) {
    std::unique_lock lock(mutex_);
    
    if (bases_.contains(base.name)) {
        return make_error<void>(ErrorCode::DUPLICATE_KEY, "GDG base exists: " + base.name);
    }
    
    GdgBase new_base = base;
    new_base.created = SystemClock::now();
    bases_[base.name] = std::move(new_base);
    generations_[base.name] = {};
    
    return make_success();
}

Result<void> GdgManager::delete_base(const String& name) {
    std::unique_lock lock(mutex_);
    
    if (!bases_.contains(name)) {
        return make_error<void>(ErrorCode::GDG_BASE_NOT_FOUND, "GDG base not found: " + name);
    }
    
    bases_.erase(name);
    generations_.erase(name);
    
    return make_success();
}

Result<GdgBase> GdgManager::get_base(const String& name) {
    std::shared_lock lock(mutex_);
    
    auto it = bases_.find(name);
    if (it == bases_.end()) {
        return make_error<GdgBase>(ErrorCode::GDG_BASE_NOT_FOUND, "GDG base not found: " + name);
    }
    
    return make_success(it->second);
}

Result<GdgGeneration> GdgManager::create_generation(const String& base_name) {
    std::unique_lock lock(mutex_);
    
    auto base_it = bases_.find(base_name);
    if (base_it == bases_.end()) {
        return make_error<GdgGeneration>(ErrorCode::GDG_BASE_NOT_FOUND, "GDG base not found");
    }
    
    auto& gens = generations_[base_name];
    UInt16 next_gen = gens.empty() ? 1 : gens.back().absolute_number + 1;
    
    // Check limit
    if (gens.size() >= base_it->second.limit) {
        // Roll off oldest
        if (base_it->second.model == GdgModel::FIFO && !gens.empty()) {
            gens.erase(gens.begin());
        }
    }
    
    GdgGeneration gen;
    gen.base_name = base_name;
    gen.generation_name = generate_generation_name(base_name, next_gen, 0);
    gen.relative_number = 0;
    gen.absolute_number = next_gen;
    gen.version = 0;
    gen.created = SystemClock::now();
    gen.active = true;
    
    // Update relative numbers
    for (auto& g : gens) {
        g.relative_number--;
    }
    
    gens.push_back(gen);
    
    return make_success(gen);
}

Result<GdgGeneration> GdgManager::get_generation(const String& base_name, Int16 relative) {
    std::shared_lock lock(mutex_);
    
    auto it = generations_.find(base_name);
    if (it == generations_.end() || it->second.empty()) {
        return make_error<GdgGeneration>(ErrorCode::GDG_GENERATION_NOT_FOUND, "No generations");
    }
    
    const auto& gens = it->second;
    
    // Find by relative number
    for (const auto& gen : gens) {
        if (gen.relative_number == relative) {
            return make_success(gen);
        }
    }
    
    return make_error<GdgGeneration>(ErrorCode::GDG_GENERATION_NOT_FOUND, "Generation not found");
}

Result<void> GdgManager::roll_off(const String& base_name) {
    std::unique_lock lock(mutex_);
    
    auto it = generations_.find(base_name);
    if (it == generations_.end() || it->second.empty()) {
        return make_error<void>(ErrorCode::GDG_ERROR, "No generations to roll off");
    }
    
    it->second.erase(it->second.begin());
    
    return make_success();
}

std::vector<GdgGeneration> GdgManager::list_generations(const String& base_name) {
    std::shared_lock lock(mutex_);
    
    auto it = generations_.find(base_name);
    return it != generations_.end() ? it->second : std::vector<GdgGeneration>{};
}

Size GdgManager::generation_count(const String& base_name) const {
    std::shared_lock lock(mutex_);
    
    auto it = generations_.find(base_name);
    return it != generations_.end() ? it->second.size() : 0;
}

} // namespace cics::gdg
