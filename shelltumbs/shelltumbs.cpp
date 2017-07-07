// shelltumbs.cpp : Defines the entry point for the console application.
//

#pragma comment(lib,"gdiplus.lib")

#include "stdafx.h"
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


int main(int argc, char *argv[])
{	

	if (argc != 3) {
		cout << "Args:  <folderpath> <thumbsize (e.g 96. 256)>";
		return 0;
	}
	int thumbsize = atoi(argv[2]);


	int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow);
	HRESULT initcode = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
		COINIT_DISABLE_OLE1DDE);
	// bail if initialisation of context fails, because the other calls won't work.
	if (FAILED(initcode)) {
		return 0;
	}

	// convert argument to the correct format
	wchar_t folderpath[_MAX_PATH] = { 0 };
	MultiByteToWideChar(0, 0, argv[1], strlen(argv[1]), folderpath, strlen(argv[1]));
	LPWSTR szFolderPath = folderpath;
	wcout << L"Folder path: " << szFolderPath << endl;
	cout << "Thumbsize: " << thumbsize << endl;


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
	ISharedBitmap* shared_bitmap = nullptr;
	WTS_CACHEFLAGS* flags = nullptr;
	HBITMAP hbitmap = NULL;
	CLSID bmpCLSid;
	CLSID jpegCLSid;

	// init GDI
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	int retVal = GetEncoderClsid(L"image/bmp", &bmpCLSid);
	retVal = GetEncoderClsid(L"image/jpeg", &jpegCLSid);

	EncoderParameters encoderParameters;
	ULONG             quality = 90;  // quality level of jpeg
	encoderParameters.Count = 1;
	encoderParameters.Parameter[0].Guid = EncoderQuality;
	encoderParameters.Parameter[0].Type = EncoderParameterValueTypeLong;
	encoderParameters.Parameter[0].NumberOfValues = 1;
	encoderParameters.Parameter[0].Value = &quality;



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
				
				// reset thumbnail objects
				shared_bitmap = nullptr;
				flags = nullptr;
				hbitmap = NULL;

				// get the thumbnail
				thumbhr = cache->GetThumbnail(pChildItem, 
					thumbsize,
					WTS_EXTRACTDONOTCACHE,   // extract but don't clog up the local cache.
					&shared_bitmap,
					flags,
					nullptr
				);
				if (SUCCEEDED(thumbhr)) {
					wprintf(L"Obtained %s - ", szChildName);
					cout << flagnames[int(flags)] << endl;
					// Thumbnail extracted, now get the image content
					thumbhr = shared_bitmap->GetSharedBitmap(&hbitmap);
					if (SUCCEEDED(thumbhr)) {
						// Do somethign with the bitmap content
						Bitmap *image = new Bitmap(hbitmap, NULL);


						// File names
						wstring fname = szChildName;
						fname = fname + L"_" + to_wstring(thumbsize);
						wstring fnamebmp = fname + L".bmp";
						wstring fnamejpeg = fname + L".jpeg";

						// Save to disk
						image->Save(fnamebmp.c_str(), &bmpCLSid, NULL);
						image->Save(fnamejpeg.c_str(), &jpegCLSid, &encoderParameters);

						delete image;
					}
				}
				else {
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

    return 0;
}

