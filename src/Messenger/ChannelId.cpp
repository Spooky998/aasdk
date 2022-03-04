#include <aasdk/Messenger/ChannelId.hpp>

namespace aasdk::messenger {

std::string channelIdToString(ChannelId channelId) {
  switch (channelId) {
    case ChannelId::CONTROL:return "CONTROL";
    case ChannelId::INPUT:return "INPUT";
    case ChannelId::SENSOR:return "SENSOR";
    case ChannelId::VIDEO:return "VIDEO";
    case ChannelId::MEDIA_AUDIO:return "MEDIA_AUDIO";
    case ChannelId::SPEECH_AUDIO:return "SPEECH_AUDIO";
    case ChannelId::SYSTEM_AUDIO:return "SYSTEM_AUDIO";
    case ChannelId::AV_INPUT:return "AV_INPUT";
    case ChannelId::BLUETOOTH:return "BLUETOOTH";
    case ChannelId::PHONE_STATUS:return "PHONE_STATUS";
    case ChannelId::NAVIGATION:return "NAVIGATION";
    case ChannelId::NONE:return "NONE";
    default:return "(null)";
  }
}

}
