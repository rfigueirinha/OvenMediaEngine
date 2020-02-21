//==============================================================================
//
//  OvenMediaEngine fork by Abyssal.SA
//
//  Created by Rui Figueirinha @ github.com/rfigueirinha
//                             @ rfigueirinha@abyssal.eu
//
//
//==============================================================================

#include "authentication.h"

Auth* Auth::instance;

Auth *Auth::GetInstance()
{
    if(instance == nullptr)
        instance = new Auth;
    return instance;
}

ov::String Auth::CalculateSHA256Hash(ov::String streamName, streamCommand command)
{
    ov::String streamDirection;
    if(command == 0){streamDirection = "publish";} else if(command == 1){streamDirection = "subscribe";}
    ov::String src_str = streamName + streamDirection + serverPassphrase;
    std::string hash_hex_std_string;
    std::string src_std_string = src_str.CStr();

    // Generate sha256 hash from streamName and server passphrase
    picosha2::hash256_hex_string(src_std_string, hash_hex_std_string);
    ov::String hash_hex_ov_string = hash_hex_std_string.c_str();
    return hash_hex_ov_string;
}


ov::String Auth::GetSHA256Hash(ov::String streamName, Auth::streamCommand command)
{
    switch(command)
    {
        case Auth::streamCommand::publish:
            // return the first value of the pair (publish hash key)
            return streams.find(streamName)->second.first;
        case Auth::streamCommand::subscribe:
            // return second value of the pair (subscribe hash key)
            return streams.find(streamName)->second.second;
        default:
            std::cout << "Stream authentication ERROR, undefined type, returning null hash";
            return nullptr;
    }
}

// Enter a stream into the authentication manager for calculating the publish/subscribe hash
void Auth::PushBackStream(ov::String streamName)
{
    // streams --> {stream name, pair<publish hash, subscribe hash>}
    // Creates the {string, pair} for each stream
    streams.emplace(streamName, std::make_pair(instance->CalculateSHA256Hash(streamName, Auth::streamCommand::publish), instance->CalculateSHA256Hash(streamName, Auth::streamCommand::subscribe)));
    // logti("Stream added to auth manager. stream name: %s pub token: %s sub token: %s", streamName.CStr(), GetSHA256Hash(streamName, Auth::streamCommand::publish).CStr(), GetSHA256Hash(streamName, Auth::streamCommand::subscribe).CStr());
    std::cout << "Stream added to auth manager. stream name: " << streamName.CStr()
     << " pub token: " << GetSHA256Hash(streamName, Auth::streamCommand::publish).CStr()
     << "sub token: " << GetSHA256Hash(streamName, Auth::streamCommand::subscribe).CStr() << std::endl;
}

bool Auth::HasAuthentication(){
    return serverPassphrase != nullptr && serverPassphrase !="";
}