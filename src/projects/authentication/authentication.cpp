//==============================================================================
//
//  OvenMediaEngine fork by Abyssal.SA
//
//  Created by Rui Figueirinha @ github.com/rfigueirinha
//                             @ rfigueirinha@abyssal.eu
//  
//
//==============================================================================
//#include <iostream>

#include "authentication.h"


// private

// public
ov::String Auth::CalculateSHA256Hash(ov::String streamName, streamCommand command)
{
    ov::String streamDirection;
    if(command == 0){streamDirection = "publish";} else if(command == 1){streamDirection = "subscribe";}
    ov::String src_str = streamName + streamDirection; // + _server_passphrase;
    ov::String hash_hex_string;

    // Generate sha256 hash from streamName and server passphrase
    picosha2::hash256_hex_string(src_str.CStr(), hash_hex_string.CStr());
    //SetSHA256Hash(hash_hex_string);

    return hash_hex_string;
}


ov::String Auth::GetSHA256Hash(ov::String streamName, Auth::streamCommand command)
{
    if(command == 0)
    {
        // return the first value of the pair (publish hash key)
        return streams.find(streamName)->second.first;
    }
    else if(command == 1)
    {
        // return second value of the pair (subscribe hash key)
        return streams.find(streamName)->second.second;
    }
}

// Enter a stream into the authentication manager for calculating the publish/subscribe hash
void Auth::PushBackStream(ov::String streamName)
{
    //auto auth = Auth::GetInstance();
    // streams --> {stream name, pair<publish hash, subscribe hash>}
    // Creates the {string, pair} for each stream
    streams.emplace(streamName, std::make_pair(instance->CalculateSHA256Hash(streamName, Auth::streamCommand::publish), instance->CalculateSHA256Hash(streamName, Auth::streamCommand::subscribe)));
}