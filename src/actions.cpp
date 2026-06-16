#include "actions.h"
#include <unordered_map>

namespace actions {
namespace {

std::unordered_map<std::string, Action>& registry() {
    static std::unordered_map<std::string, Action> r;
    return r;
}

}  // namespace

void register_action(const std::string& name, Action action) {
    registry()[name] = std::move(action);
}

const Action* find(const std::string& name) {
    auto it = registry().find(name);
    return it == registry().end() ? nullptr : &it->second;
}

}  // namespace actions
