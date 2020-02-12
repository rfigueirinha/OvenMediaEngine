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
#include "picosha2.h"


// private
void Auth::SetSHA256Hash(std::string hash)
{
    _sha256hash = hash;
}

//public
std::string Auth::CalculateSHA256Hash(std::string streamName)
{
    std::string src_str = streamName + _passphrase;
    std::string hash_hex_string;

    // Generate sha256 hash from streamName and server passphrase
    picosha2::hash256_hex_string(src_str, hash_hex_string);
    SetSHA256Hash(hash_hex_string);
}


std::string Auth::GetSHA256Hash() const
{
    return _sha256hash;
}