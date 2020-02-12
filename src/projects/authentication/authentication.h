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

#include <pugixml-1.9/src/pugixml.hpp> // XML Read
#include <string>

class Auth
{

private:
    // sha256 hash string
    // It's calculated using a secret password and the stream name
    static std::string _sha256hash;
    std::string _passphrase; 
    void SetSHA256Hash(std::string hash);
public:
    std::string streamName; // Each Auth instance has a streamName for generating an unique hash token
    std::string CalculateSHA256Hash(std::string streamName);
    
    std::string GetSHA256Hash() const;

    Auth(std::string streamname) : streamName(streamname){};
};