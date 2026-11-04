#include "app/Commands.h"

namespace app {

const char* commandName(CommandType type) {
    switch (type) {
    case CommandType::Discover: return "Discover";
    case CommandType::LoadFavorites: return "LoadFavorites";
    case CommandType::RefreshStatus: return "RefreshStatus";
    case CommandType::PlayFavorite: return "PlayFavorite";
    case CommandType::SetVolume: return "SetVolume";
    case CommandType::Stop: return "Stop";
    case CommandType::LoadLogo: return "LoadLogo";
    }
    return "?";
}

} // namespace app
