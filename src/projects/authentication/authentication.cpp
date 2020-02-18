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

Auth* Auth::instance;

// public
Auth *Auth::GetInstance()
{
    if(instance == nullptr)
        instance = new Auth;
    return instance;
}

// streamName + streamDirection + passphrase (passphrase in Server.xml)
ov::String Auth::CalculateSHA256Hash(ov::String streamName, streamCommand command)
{
    ov::String streamDirection;
    if(command == 0){streamDirection = "publish";} else if(command == 1){streamDirection = "subscribe";}
    ov::String src_str = streamName + streamDirection + serverPassphrase;
    //logte("Source String is %s", src_str.CStr());
    std::string hash_hex_std_string;
    std::string src_std_string = src_str.CStr();
    // Generate sha256 hash from streamName and server passphrase
    picosha2::hash256_hex_string(src_std_string, hash_hex_std_string);

    //std::cout << hash_hex_std_string << std::endl;

    ov::String hash_hex_ov_string = hash_hex_std_string.c_str();
    return hash_hex_ov_string;
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
    // streams --> {stream name, pair<publish hash, subscribe hash>}
    // Creates the {string, pair} for each stream    
    streams.emplace(streamName, std::make_pair(instance->CalculateSHA256Hash(streamName, Auth::streamCommand::publish), instance->CalculateSHA256Hash(streamName, Auth::streamCommand::subscribe)));
    // logti("Stream added to auth manager. stream name: %s pub token: %s sub token: %s", streamName.CStr(), GetSHA256Hash(streamName, Auth::streamCommand::publish).CStr(), GetSHA256Hash(streamName, Auth::streamCommand::subscribe).CStr());
    std::cout << "Stream added to auth manager. stream name: " << streamName.CStr()
     << " pub token: " << GetSHA256Hash(streamName, Auth::streamCommand::publish).CStr() 
     << "sub token: " << GetSHA256Hash(streamName, Auth::streamCommand::subscribe).CStr() << std::endl;
}