//==============================================================================
//
//  OvenMediaEngine fork by Abyssal.SA
//
//  Created by Rui Figueirinha @ github.com/rfigueirinha
//                             @ rfigueirinha@abyssal.eu
//  
//
//==============================================================================

#pragma once

#include "picosha2.h"

#include <string>
#include <map>

extern std::string _server_passphrase;

class Auth
{
private:
    // sha256 hash string
    // It's calculated using a secret password (configured in Server.xml), stream direction (publish/subscribe) and the stream name
    //std::string _sha256hash;
    //std::string _passphrase; 
    void SetSHA256Hash(std::string hash);
    Auth(){};
    static Auth* instance;
public:
    // 0 for publish, 1 for subscribe
    enum streamCommand
    {
        publish ,
        subscribe,
    };
    std::string streamName; // Each Auth instance has a streamName for generating an unique hash token
    std::string CalculateSHA256Hash(std::string streamName, streamCommand command);
    static std::string GetSHA256Hash(std::string streamName, Auth::streamCommand command);

    // map that holds the tuples of the streams
    static std::map<std::string, std::pair<std::string, std::string>> streams;
    
 
    
    void PushBackStream(std::string streamName);
};
