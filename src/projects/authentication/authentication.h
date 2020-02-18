//==============================================================================
//
//  OvenMediaEngine fork by Abyssal.SA
//
//  Created by Rui Figueirinha @ github.com/rfigueirinha
//                             @ rfigueirinha@abyssal.eu
//  
//
//==============================================================================
// Singleton class that represents the authentication of the server


#pragma once

#include "picosha2.h"
#include "../base/ovlibrary/string.h"

#include <iostream>
#include <utility>
#include <string>
#include <map>

//extern ov::String _server_passphrase;

class Auth
{
public:
    static Auth *GetInstance();

    // 0 for publish, 1 for subscribe
    enum streamCommand
    {
        publish ,
        subscribe,
    };

    ov::String serverPassphrase;
    ov::String streamName; // Each Auth instance has a streamName for generating an unique hash token
    ov::String CalculateSHA256Hash(ov::String streamName, streamCommand command);
    ov::String GetSHA256Hash(ov::String streamName, Auth::streamCommand command);

    // map that holds the tuples of the streams
    std::map<ov::String, std::pair<ov::String, ov::String>> streams;
    
    void PushBackStream(ov::String streamName);

private:
    // sha256 hash string
    // It's calculated using a secret password (configured in Server.xml), stream direction (publish/subscribe) and the stream name
    //std::string _sha256hash;
    //std::string _passphrase; 
    void SetSHA256Hash(ov::String hash);
    static Auth *instance;
    Auth(){};
    ~Auth(){};
};