/* shelltumbs.cpp :  An application to create Windows thumbnails for a given directory and output them to a file. Output files retain their original name, even if the file type changes (i.e. from jpeg to bitmap for the 96x96 thumbnails).
Items are cached and then retrieved directly from the thumbcache such that binary identity with actual thumbcache entries is preserved.

As parsing the cache every time an image is thumbnailed is expensive, thumbnails are recovered in batches from the thumbcache. This can be controlled by changing the <batchsize> argument.
Note that if batch size is larger than the number of items in the target directory, nothing will be saved. Make sure that all items are processed by selecting batch size such that:
num_files_in_directory % batchsize == 0

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

	if (argc != 6) {
		cout << "Args:  <folderpath> <thumbcachedb_path> <outpath> <thumbsize (e.g 96. 256)> <batchsize>" << endl;
		cout << "Specified thumbcachedb should match the thumbsize" << endl;
		cout << "Example: C:\\imagedir C:\\Users\\DefaultUser\\AppData\\Local\\Microsoft\\Windows\\Explorer\\thumbcache_256.db C:\\outpath 256 5000" << endl;
		return 0;
	}

	clock_t start0, end0;
	time_t start1, end1;
	start1 = time(NULL);
	start0 = clock();


	// Parse arguments.
	wchar_t * folderpath = argv[1];
	wchar_t * dbname = argv[2];
	wchar_t * output_path = argv[3];
	int thumbsize = _wtoi(argv[4]);
	int batchsize = _wtoi(argv[5]);


	int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow);
	HRESULT initcode = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
		COINIT_DISABLE_OLE1DDE);
	// bail if initialisation of context fails, because the other calls won't work.
	if (FAILED(initcode)) {
		return 0;
	}

	cout << "Extracting " << thumbsize << " pixel thumbnails in batches of " << batchsize << endl;

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
					//wprintf(L"%s - ", szChildName);
					//cout << flagnames[int(flags)] << " - ";


					// Get CacheID ("Cache Entry Hash" in ThumbCache Viewer).
					// Viewer reverses the bytes and ignores the half which is comprised of zeroes.
					// Output here is set to match the viewer.
					
					stringstream id;
					id << "0x";
					for (int i = 7; i>-1; i--) {
						id << setw(2) << setfill('0') << hex << (int)thumbid.rgbKey[i];
					}

					idmap[id.str()] = 1;
					nameidmap[id.str()] = szChildName;
					//wcout << nameidmap[id.str()] << endl;
					if (idmap.size() == batchsize){
						i = i + batchsize;
						cout << "Cached: " << i << ", parsing thumcache.db ...";
						// Parse thumbcache and save entries which have IDs matching those in idmap to output_path
						idmap = exportThumbs(dbname, output_path, idmap, nameidmap);
						// Output if any items failed to be retrieved, shouldn't happen for batch sizes << thumbcache max size.
						if (idmap.size() > 0) {
							cout << "failed to save: " << idmap.size() << endl;
						}
						else {
							cout << "success" << endl;
						}
						

						for (auto kv : idmap) {
							cout << kv.first << endl;
							failedIds.push_back(kv.first);   //keep track of failed items
							//vals.push_back(kv.second);
						}
						idmap.clear(); // reset the IDs to get from the cache, these one's arent there.
						nameidmap.clear(); // reset id-filename map.
					}
				

				} else {
					wprintf(L"Failed to obtain %s\n", szChildName);
				}

				CoTaskMemFree(szChildName);
				pChildItem->Release();
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

