// benchmarkLookup.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "lookupThumbcache.h"




using namespace std;

int wmain(int argc, wchar_t *argv[])
{

	if (argc != 6) {
		cout << "Args: <thumbcachedb_path> <sha256file> <crc64file>";
		return 0;
	}


	// Parse arguments.
	wchar_t * dbpath = argv[1];
	wchar_t * shapath = argv[2];
	wchar_t * crcpath = argv[3];


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

	// Populate CRC64 map
	unordered_map <string, int> crchecks;
	ifstream mapinput(crcpath);
	if (!mapinput.is_open()) {
		cout << "Couldn't open " << crcpath << endl;
		return 1;
	}
	for (string line; getline(mapinput, line); )
	{
		line.erase(line.length());

		crchecks[line] = 1;
	}


	// Start timer - Don't benchmark lookup initialisation above as they would be hard coded.
	clock_t start0, end0;
	time_t start1, end1;
	start1 = time(NULL);
	start0 = clock();


	// Do call here
	vector<string> found = lookupThumbs(dbpath, &shasigs, &crchecks);
	double cputime = (double)(end0 - start0) / (CLOCKS_PER_SEC);
	double walltime = difftime(end1, start1);

	cout << "Identified " << found.size() << " items." << endl;
	for (int i = 0; i < found.size(); i++) {
		cout << i << ":" << found[i] << endl;
	}


	printf("done | cputime %.3fs | walltime %.3fs\n", cputime, walltime);
	cout << walltime << endl;

	return 0;
}

