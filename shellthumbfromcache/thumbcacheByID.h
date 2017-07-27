#pragma once
#ifndef THUMBCACHEBYID    // To make sure you don't declare the function more than once by including the header multiple times.
#define THUMBCACHEBYID

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <unordered_map>
#include <string>
#include <fstream>
#include <iostream>
#include <vector>


std::unordered_map<std::string, int> exportThumbs(wchar_t* dbname, wchar_t * output_path, std::unordered_map<std::string, int> ids, std::unordered_map<std::string, std::wstring> nameidmap);

#endif