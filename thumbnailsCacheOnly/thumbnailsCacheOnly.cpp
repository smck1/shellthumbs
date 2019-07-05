/* shelltumbs.cpp :  An application to cache files from a given directory in the Windows thumbcache. Caching is forced, such that items which are already cached will be cached again.


*/

#pragma comment(lib,"gdiplus.lib")

#include "stdafx.h"


using namespace Gdiplus;
using namespace std;

// GetThumbnail flag mappings.
string flagnames[3] = {
	"WTS_DEFAULT",
	"WTS_LOWQUALITY",
	"WTS_CACHED" };


int wmain(int argc, wchar_t *argv[])
{

	if (argc != 4) {
		cout << "Args:  <folderpath> <thumbcachedb_path> <thumbsize (e.g 96. 256)>" << endl;
		cout << "Specified thumbcachedb should match the thumbsize" << endl;
		cout << "Example: C:\\imagedir C:\\Users\\DefaultUser\\AppData\\Local\\Microsoft\\Windows\\Explorer\\thumbcache_256.db 256" << endl;
		return 0;
	}

	clock_t start0, end0;
	time_t start1, end1;
	start1 = time(NULL);
	start0 = clock();


	// Parse arguments.
	wchar_t * folderpath = argv[1];
	wchar_t * dbname = argv[2];
	int thumbsize = _wtoi(argv[3]);


	int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow);
	HRESULT initcode = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
		COINIT_DISABLE_OLE1DDE);
	// bail if initialisation of context fails, because the other calls won't work.
	if (FAILED(initcode)) {
		return 0;
	}

	cout << "Extracting " << thumbsize << " pixel thumbnails." << endl;

	// Get the shellitem for the folder specified
	LPWSTR szFolderPath = folderpath;
	IShellItem* pItem = nullptr;
	HRESULT hr = ::SHCreateItemFromParsingName(
		szFolderPath, 0, IID_PPV_ARGS(&pItem));
	if (SUCCEEDED(hr))
	{
		LPWSTR szName = nullptr;
		hr = pItem->GetDisplayName(SIGDN_NORMALDISPLAY, &szName);
		if (SUCCEEDED(hr))
		{
			//wprintf(L"Shell item name: %s\n", szName);
			::CoTaskMemFree(szName);

		}
	}
	if (FAILED(hr)) {
		cout << "failed: ";
		printf("%X", hr);
	}


	// init com object for thumnailcache
	IThumbnailCache* cache = nullptr;
	hr = CoCreateInstance(CLSID_LocalThumbnailCache, nullptr, CLSCTX_INPROC, IID_PPV_ARGS(&cache));
	if (FAILED(hr)) {
		cout << "Failed to acquire local thumbnail cache reference, aborting." << endl;
		return 0;
	}

	//inmitialise objects for thumbnail extraction
	HRESULT thumbhr = CoInitialize(nullptr);
	WTS_CACHEFLAGS flags;
	WTS_THUMBNAILID thumbid;

	// Tracking of ids and filenames.
	unordered_map<string, int> idmap;
	vector<string> failedIds;
	unordered_map<string, wstring> nameidmap;
	int i = 0;

	// Iterate through files in the folder
	IEnumShellItems* pEnum = nullptr;
	hr = pItem->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&pEnum));
	if (SUCCEEDED(hr))
	{
		IShellItem* pChildItem = nullptr;
		ULONG ulFetched = 0;
		do
		{
			hr = pEnum->Next(1, &pChildItem, &ulFetched);
			if (FAILED(hr)) break;
			if (ulFetched != 0)
			{
				LPWSTR szChildName = nullptr;
				pChildItem->GetDisplayName(SIGDN_NORMALDISPLAY, &szChildName);

				// Extract the thumbnail from the original via the Windows Shell API
				thumbhr = cache->GetThumbnail(pChildItem,
					thumbsize,
					WTS_FORCEEXTRACTION,   // force extraction
					NULL, // Don't retrieve the memory mapped bitmap.
					&flags,
					&thumbid
				);
				if (SUCCEEDED(thumbhr)) {

					i++;

				}
				else {
					wprintf(L"Failed to obtain %s\n", szChildName);
				}

				CoTaskMemFree(szChildName);
				pChildItem->Release();
			}
			if (i % 1000 == 0) {
				cout << "Cached " << i << " items" << endl;
			}
		} while (hr != S_FALSE);
		pEnum->Release();
	}

	// Clean up
	pItem->Release();

	end0 = clock();
	end1 = time(NULL);

	if (failedIds.size() > 0) {
		cout << "Failed IDs" << endl;
		// Print failed IDs
		for (int x = 0; x < failedIds.size(); x++) {
			wcout << nameidmap[failedIds[x]] << endl;
		}
	}


	double cputime = (double)(end0 - start0) / (CLOCKS_PER_SEC);
	double walltime = (double)(end1 - start1);


	printf("done | cputime %.3fs | walltime %.3fs\n", cputime, walltime);

	return 0;
}

