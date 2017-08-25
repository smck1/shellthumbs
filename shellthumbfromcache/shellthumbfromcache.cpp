/* shelltumbs.cpp :  An application to create Windows thumbnails for a given directory and output them to a file. Output files retain their original name, even if the file type changes (i.e. from jpeg to bitmap for the 96x96 thumbnails).
Items are cached and then retrieved directly from the cache such that binary identity is preserved, with image binaries being saved as they normally would.
As parsing the cache every time an image is thumbnailed is expensive, thumbnails are recovered in batches from the cache itself. This can be controlled by changing the "batchsize" argument.
Note that if batch size is larger than the number of items in the target directory, nothing will be saved. Make sure that all items are processed by selecting batch size such that:
no_files_indirectory % batchsize == 0

*/

#pragma comment(lib,"gdiplus.lib")

#include "stdafx.h"
#include "thumbcacheByID.h"
#include <sstream>
#include <unordered_map>
//#include <GdiplusHelperFunctions.h>

using namespace Gdiplus;
using namespace std;


//LPWSTR chartowchar(char* inchars) {
//	std::string schars = std::string(inchars);
//	std::wstring w1 = std::wstring(schars.begin(), schars.end());
//	const wchar_t* w2 = w1.c_str();
//	
//	
//}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT num = 0;//number of image encoders 
	UINT size = 0;//size of the image encoder array in bytes 

	ImageCodecInfo* pImageCodecInfo = NULL;

	GetImageEncodersSize(&num, &size);
	if (size == 0)
		return-1;//Failure 

	pImageCodecInfo = (ImageCodecInfo *)(malloc(size));
	if (pImageCodecInfo == NULL)
		return-1;//Failure 

	GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j <num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;//Success 
		}
	}

	free(pImageCodecInfo);
	return-1;//Failure 
}

string flagnames[3] = {
	"WTS_DEFAULT",
	"WTS_LOWQUALITY",
	"WTS_CACHED" };


int wmain(int argc, wchar_t *argv[])
{

	if (argc != 6) {
		cout << "Args:  <folderpath> <dbpath> <outpath> <thumbsize (e.g 96. 256)> <batchsize>";
		return 0;
	}

	wchar_t folderpath[MAX_PATH] = { 0 };
	wchar_t dbname[MAX_PATH] = { 0 };
	wchar_t output_path[MAX_PATH] = { 0 };
	int thumbsize = _wtoi(argv[4]);
	int batchsize = _wtoi(argv[5]);


	int arg_len = wcslen(argv[1]);
	wmemcpy_s(folderpath, MAX_PATH, argv[1], (arg_len > MAX_PATH ? MAX_PATH : arg_len));
	arg_len = wcslen(argv[2]);
	wmemcpy_s(dbname, MAX_PATH, argv[2], (arg_len > MAX_PATH ? MAX_PATH : arg_len));
	arg_len = wcslen(argv[3]);
	wmemcpy_s(output_path, MAX_PATH, argv[3], (arg_len > MAX_PATH ? MAX_PATH : arg_len));





	int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow);
	HRESULT initcode = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
		COINIT_DISABLE_OLE1DDE);
	// bail if initialisation of context fails, because the other calls won't work.
	if (FAILED(initcode)) {
		return 0;
	}


	LPWSTR szFolderPath = folderpath;
	wcout << L"Folder path: " << szFolderPath << endl;
	//wcout << L"DBfile: " << szFolderPath << endl;
	cout << "Thumbsize: " << thumbsize << endl;
	wcout << L"Output path:" << output_path << endl;


	// Get the shellitem for the folder specified
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
	//ISharedBitmap* shared_bitmap = nullptr;
	WTS_CACHEFLAGS flags;
	WTS_THUMBNAILID thumbid;
	//HBITMAP hbitmap = NULL;
	CLSID bmpCLSid;
	CLSID jpegCLSid;

	// init GDI
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	int retVal = GetEncoderClsid(L"image/bmp", &bmpCLSid);
	retVal = GetEncoderClsid(L"image/jpeg", &jpegCLSid);



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

				// get the thumbnail
				thumbhr = cache->GetThumbnail(pChildItem,
					thumbsize,
					WTS_FORCEEXTRACTION,   // force extraction
					NULL,
					&flags,
					&thumbid
				);
				if (SUCCEEDED(thumbhr)) {
					//wprintf(L"%s - ", szChildName);
					//cout << flagnames[int(flags)] << " - ";


					// Get CacheID (Cache Entry Hash) in ThumbCache Viewer.
					// Viewer reverses the bytes and ignores the half which is comprised of zeroes.
					// Output here is set to match the viewer.
					
					stringstream id;
					id << "0x";
					for (int i = 7; i>-1; i--) {
						//cout << setw(2) << setfill('0') << hex << (int)thumbid.rgbKey[i];
						id << setw(2) << setfill('0') << hex << (int)thumbid.rgbKey[i];
					}
					//cout << endl;
					//cout << id.str() << endl;
					idmap[id.str()] = 1;
					nameidmap[id.str()] = szChildName;
					//wcout << nameidmap[id.str()] << endl;
					if (idmap.size() == batchsize){
						i = i + batchsize;
						cout << "Acquired: " << i << endl;
						idmap = exportThumbs(dbname, output_path, idmap, nameidmap);
						cout << "failed to get: " << idmap.size() << endl;

						for (auto kv : idmap) {
							cout << kv.first << endl;
							failedIds.push_back(kv.first);   //keep track of names
							//vals.push_back(kv.second);
						}
						idmap.clear(); //reset the IDs to get from the cache, these one's arent there.
						nameidmap.clear();
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
	GdiplusShutdown(gdiplusToken);
	pItem->Release();
	
	if (failedIds.size() > 0) {
		cout << "Failed IDs" << endl;
		for (int x = 0; x < failedIds.size(); x++) {
			wcout << nameidmap[failedIds[x]] << endl;
		}
	}


	//cout << "Name ID Map" << endl;
	//for (auto kv : nameidmap) {
	//	//key first, value second
	//	cout << kv.first;
	//	wcout << " " << kv.second << endl;
	//}



	return 0;
}

