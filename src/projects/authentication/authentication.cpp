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
std::string Auth::CalculateSHA256Hash(std::string streamName, streamCommand command)
{
    std::string streamDirection;
    if(command == 0){streamDirection = "publish";} else if(command == 1){streamDirection = "subscribe";}
    std::string src_str = streamName + streamDirection + _server_passphrase;
    std::string hash_hex_string;

    // Generate sha256 hash from streamName and server passphrase
    picosha2::hash256_hex_string(src_str, hash_hex_string);
    //SetSHA256Hash(hash_hex_string);

    return hash_hex_string;
}


std::string Auth::GetSHA256Hash(std::string streamName, Auth::streamCommand command)
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
void Auth::PushBackStream(std::string streamName)
{
    // streams --> {stream name, pair<publish hash, subscribe hash>}
    // Creates the {string, pair} for each stream
    streams.emplace(streamName, std::make_pair(CalculateSHA256Hash(streamName, Auth::streamCommand::publish), CalculateSHA256Hash(streamName, Auth::streamCommand::publish)));
}