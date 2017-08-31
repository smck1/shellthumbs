#pragma once
#ifndef LOOKTHUMBCACHEBYID    // To make sure you don't declare the function more than once by including the header multiple times.
#define LOOKTHUMBCACHEBYID

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <unordered_map>
#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <iostream>
#include <sstream>




std::vector<std::pair<std::string, std::string>> lookupThumbs(wchar_t* dbname, std::unordered_map<std::string, int> * sha256hashes, std::unordered_map<std::string, int> * crc64checksums);

#endif