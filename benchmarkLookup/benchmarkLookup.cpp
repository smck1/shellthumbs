// benchmarkLookup.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <chrono>
#include "lookupThumbcache.h"




using namespace std;

int wmain(int argc, wchar_t *argv[])
{
	if (argc<3 || argc>4) {
		cout << "Args: <thumbcachedb_path> <sha256file> <optional: crc64file>" << endl;;
		return 0;
	}


	// Parse arguments.
	wchar_t * dbpath = argv[1];
	wchar_t * shapath = argv[2];
	wchar_t * crcpath;

	if (argc == 4) {
		crcpath = argv[3];
	}
	else {
		crcpath = NULL;
	}
	


	// Populate SHA256 map
	unordered_map <string, int> shasigs;
	ifstream mapinput(shapath);
	if (!mapinput.is_open()) {
		cout << "Couldn't open " << shapath << endl;
		return 1;
	}
	for (string line; getline(mapinput, line); )
	{
		line.erase(line.length());
		shasigs[line] = 1;
	}


	// Populate CRC64 map if input provided.
	unordered_map <string, int> crchecks;
	if (crcpath != NULL) {
		ifstream mapinput2(crcpath);
		if (!mapinput2.is_open()) {
			cout << "Couldn't open " << crcpath << endl;
			return 1;
		}
		for (string line; getline(mapinput2, line); )
		{
			line.erase(line.length());

			crchecks[line] = 1;
		}
	}

	// Start timer
	std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

	// Do call here
	vector<std::pair<std::string, std::string>> found;
	if (crcpath == NULL){
		found = lookupThumbs(dbpath, &shasigs, NULL);
	}
	else {
		found = lookupThumbs(dbpath, &shasigs, &crchecks);
	}

	

	cout << "Identified " << found.size() << " items." << endl;
	cout << "No.     SHA256                                                           CRC64" << endl;
	for (int i = 0; i < found.size(); i++) {
		cout << i+1 << ":      " << found[i].first << " " << found[i].second << endl;
	}

	std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
	cout << "Time taken: " <<time_span.count() <<"s" << endl;
	//cout << time_span.count() << endl;
	return 0;
}

