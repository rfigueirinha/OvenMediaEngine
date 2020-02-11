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

#include <pugixml-1.9/src/pugixml.hpp> // Leitura de XML
#include <string>

class Auth
{

private:
    // sha256 hash string
    // It's calculated using a secret password and the stream name
    std::string _sha256hash;
    std::string _passphrase; 
    void SetSHA256Hash();
public:
    std::string CalculateSHA256Hash(std::string streamName);
    
    std::string GetSHA256Hash() const;

};