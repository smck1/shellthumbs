/*
	Modified code from thumbcache_viewer_cmd (https://github.com/thumbcacheviewer/thumbcacheviewer)
	to wrap parsing in a function which only extracts thumbnails which are passed in an ID map. 
	Changes largely involve commenting out print statements and using maps to keep track of which 
	cache entries (IDs) should be saved to an output directory.
	
	Original copyright and license notice below.
	-------
    thumbcache_viewer_cmd will extract thumbnail images from thumbcache database files.
    Copyright (C) 2011-2016 Eric Kutcher

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include "stdafx.h"
#include "thumbcacheByID.h"


// Magic identifiers for various image formats.
#define FILE_TYPE_BMP	"BM"
#define FILE_TYPE_JPEG	"\xFF\xD8\xFF\xE0"
#define FILE_TYPE_PNG	"\x89\x50\x4E\x47\x0D\x0A\x1A\x0A"

// Database version.
#define WINDOWS_VISTA	0x14
#define WINDOWS_7		0x15
#define WINDOWS_8		0x1A
#define WINDOWS_8v2		0x1C
#define WINDOWS_8v3		0x1E
#define WINDOWS_8_1		0x1F
#define WINDOWS_10		0x20

// Thumbcache header information.
struct database_header
{
	char magic_identifier[ 4 ];
	unsigned int version;
	unsigned int type;	// Windows Vista & 7: 00 = 32, 01 = 96, 02 = 256, 03 = 1024, 04 = sr
};						// Windows 8: 00 = 16, 01 = 32, 02 = 48, 03 = 96, 04 = 256, 05 = 1024, 06 = sr, 07 = wide, 08 = exif
						// Windows 8.1: 00 = 16, 01 = 32, 02 = 48, 03 = 96, 04 = 256, 05 = 1024, 06 = 1600, 07 = sr, 08 = wide, 09 = exif, 0A = wide_alternate
						// Windows 10: 00 = 16, 01 = 32, 02 = 48, 03 = 96, 04 = 256, 05 = 768, 06 = 1280, 07 = 1920, 08 = 2560, 09 = sr, 0A = wide, 0B = exif, 0C = wide_alternate, 0D = custom_stream

// Found in WINDOWS_VISTA/7/8 databases.
struct database_header_entry_info
{
	unsigned int first_cache_entry;
	unsigned int available_cache_entry;
	unsigned int number_of_cache_entries;
};

// Found in WINDOWS_8v2 databases.
struct database_header_entry_info_v2
{
	unsigned int unknown;
	unsigned int first_cache_entry;
	unsigned int available_cache_entry;
	unsigned int number_of_cache_entries;
};

// Found in WINDOWS_8v3/8_1/10 databases.
struct database_header_entry_info_v3
{
	unsigned int unknown;
	unsigned int first_cache_entry;
	unsigned int available_cache_entry;
};

// Window 7 Thumbcache entry.
struct database_cache_entry_7
{
	char magic_identifier[ 4 ];
	unsigned int cache_entry_size;
	long long entry_hash;
	unsigned int filename_length;
	unsigned int padding_size;
	unsigned int data_size;
	unsigned int unknown;
	long long data_checksum;
	long long header_checksum;
};

// Window 8 Thumbcache entry.
struct database_cache_entry_8
{
	char magic_identifier[ 4 ];
	unsigned int cache_entry_size;
	long long entry_hash;
	unsigned int filename_length;
	unsigned int padding_size;
	unsigned int data_size;
	unsigned int width;
	unsigned int height;
	unsigned int unknown;
	long long data_checksum;
	long long header_checksum;
};

// Windows Vista Thumbcache entry.
struct database_cache_entry_vista
{
	char magic_identifier[ 4 ];
	unsigned int cache_entry_size;
	long long entry_hash;
	wchar_t extension[ 4 ];
	unsigned int filename_length;
	unsigned int padding_size;
	unsigned int data_size;
	unsigned int unknown;
	long long data_checksum;
	long long header_checksum;
};

bool scan_memory( HANDLE hFile, unsigned int &offset )
{
	// Allocate a 32 kilobyte chunk of memory to scan. This value is arbitrary.
	char *buf = ( char * )malloc( sizeof( char ) * 32768 );
	char *scan = NULL;
	DWORD read = 0;

	while ( true )
	{
		// Begin reading through the database.
		ReadFile( hFile, buf, sizeof( char ) * 32768, &read, NULL );
		if ( read <= 4 )
		{
			free( buf );
			return false;
		}

		// Binary string search. Look for the magic identifier.
		scan = buf;
		while ( read-- > 4 && memcmp( scan++, "CMMM", 4 ) != 0 )
		{
			++offset;
		}

		// If it's not found, then we'll keep scanning.
		if ( read < 4 )
		{
			// Adjust the offset back 4 characters (in case we truncated the magic identifier when reading).
			SetFilePointer( hFile, offset, NULL, FILE_BEGIN );
			// Keep scanning.
			continue;
		}

		break;
	}

	free( buf );
	return true;
}

/*
Parse the thumbcache at path <dbname>. For each entry, check to see if the cache ID is in <ids>, if so, save it to  <output_path> with the associated filename found in <nameidmap>.
Return any IDs which were not found (or failed to save) in the modified <ids> map.
*/
std::unordered_map<std::string, int> exportThumbs(wchar_t* dbname, wchar_t * output_path, std::unordered_map<std::string, int> ids, std::unordered_map<std::string, std::wstring> nameidmap)
{
	bool output_html = false;
	bool output_csv = false;
	bool skip_blank = false;
	bool extract_thumbnails = true;

	// Attempt to open our database file.
	HANDLE hFile = CreateFile( dbname, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		DWORD read = 0;
		DWORD written = 0;

		unsigned int file_offset = 0;

		int utf8_path_length = 0;
		char *utf8_path = NULL;
		int utf8_name_length = 0;
		char *utf8_name = NULL;
		
		HANDLE hFile_html = INVALID_HANDLE_VALUE;
		HANDLE hFile_csv = INVALID_HANDLE_VALUE;

		database_header dh = { 0 };
		ReadFile( hFile, &dh, sizeof( database_header ), &read, NULL );

		// Make sure it's a thumbcache database and the structure was filled correctly.
		if ( memcmp( dh.magic_identifier, "CMMM", 4 ) != 0 || read != sizeof( database_header ) )
		{
			CloseHandle( hFile );
			printf( "The file is not a thumbcache database.\n" );
			exit(1);
		}

		//printf( "---------------------------------------------\n" );
		//printf( "Extracting file header (%s bytes).\n", ( dh.version != WINDOWS_8v2 ? "24" : "28" ) );
		//printf( "---------------------------------------------\n" );

		// Magic identifer.
		char stmp[ 5 ] = { 0 };
		memcpy( stmp, dh.magic_identifier, sizeof( char ) * 4 );
		//printf( "Signature (magic identifier): %s\n", stmp );

		// Version of database.
		if ( dh.version == WINDOWS_VISTA )
		{
			//printf( "Version: Windows Vista\n" );
		}
		else if ( dh.version == WINDOWS_7 )
		{
			//printf( "Version: Windows 7\n" );
		}
		else if ( dh.version == WINDOWS_8 || dh.version == WINDOWS_8v2 || dh.version == WINDOWS_8v3 )
		{
			//printf( "Version: Windows 8\n" );
		}
		else if ( dh.version == WINDOWS_8_1 )
		{
			//printf( "Version: Windows 8.1\n" );
		}
		else if ( dh.version == WINDOWS_10 )
		{
			//printf( "Version: Windows 10\n" );
		}
		else
		{
			CloseHandle( hFile );
			printf( "The thumbcache database version is not supported.\n" );
			exit(1);
		}

		// Type of thumbcache database.
		//if ( dh.version == WINDOWS_VISTA || dh.version == WINDOWS_7 )	// Windows Vista & 7: 00 = 32, 01 = 96, 02 = 256, 03 = 1024, 04 = sr
		//{
		//	if ( dh.type == 0x00 )
		//	{
		//		printf( "Cache type: thumbcache_32.db, 32x32\n" );
		//	}
		//	else if ( dh.type == 0x01 )
		//	{
		//		printf( "Cache type: thumbcache_96.db, 96x96\n" );
		//	}
		//	else if ( dh.type == 0x02 )
		//	{
		//		printf( "Cache type: thumbcache_256.db, 256x256\n" );
		//	}
		//	else if ( dh.type == 0x03 )
		//	{
		//		printf( "Cache type: thumbcache_1024.db, 1024x1024\n" );
		//	}
		//	else if ( dh.type == 0x04 )
		//	{
		//		printf( "Cache type: thumbcache_sr.db\n" );
		//	}
		//	else
		//	{
		//		printf( "Cache type: Unknown\n" );
		//	}
		//}
		//else if ( dh.version == WINDOWS_10 )
		//{
		//	if ( dh.type == 0x00 )
		//	{
		//		printf( "Cache type: thumbcache_16.db, 16x16\n" );
		//	}
		//	else if ( dh.type == 0x01 )
		//	{
		//		printf( "Cache type: thumbcache_32.db, 32x32\n" );
		//	}
		//	else if ( dh.type == 0x02 )
		//	{
		//		printf( "Cache type: thumbcache_48.db, 48x48\n" );
		//	}
		//	else if ( dh.type == 0x03 )
		//	{
		//		printf( "Cache type: thumbcache_96.db, 96x96\n" );
		//	}
		//	else if ( dh.type == 0x04 )
		//	{
		//		printf( "Cache type: thumbcache_256.db, 256x256\n" );
		//	}
		//	else if ( dh.type == 0x05 )
		//	{
		//		printf( "Cache type: thumbcache_768.db, 768x768\n" );
		//	}
		//	else if ( dh.type == 0x06 )
		//	{
		//		printf( "Cache type: thumbcache_1280.db, 1280x1280\n" );
		//	}
		//	else if ( dh.type == 0x07 )
		//	{
		//		printf( "Cache type: thumbcache_1920.db, 1920x1920\n" );
		//	}
		//	else if ( dh.type == 0x08 )
		//	{
		//		printf( "Cache type: thumbcache_2560.db, 2560x2560\n" );
		//	}
		//	else if ( dh.type == 0x09 )
		//	{
		//		printf( "Cache type: thumbcache_sr.db\n" );
		//	}
		//	else if ( dh.type == 0x0A )
		//	{
		//		printf( "Cache type: thumbcache_wide.db\n" );
		//	}
		//	else if ( dh.type == 0x0B )
		//	{
		//		printf( "Cache type: thumbcache_exif.db\n" );
		//	}
		//	else if ( dh.type == 0x0C )
		//	{
		//		printf( "Cache type: thumbcache_wide_alternate.db\n" );
		//	}
		//	else if ( dh.type == 0x0D )
		//	{
		//		printf( "Cache type: thumbcache_custom_stream.db\n" );
		//	}
		//	else
		//	{
		//		printf( "Cache type: Unknown\n" );
		//	}
		//}
		//else if ( dh.version == WINDOWS_8_1 )
		//{
		//	if ( dh.type == 0x00 )
		//	{
		//		printf( "Cache type: thumbcache_16.db, 16x16\n" );
		//	}
		//	else if ( dh.type == 0x01 )
		//	{
		//		printf( "Cache type: thumbcache_32.db, 32x32\n" );
		//	}
		//	else if ( dh.type == 0x02 )
		//	{
		//		printf( "Cache type: thumbcache_48.db, 48x48\n" );
		//	}
		//	else if ( dh.type == 0x03 )
		//	{
		//		printf( "Cache type: thumbcache_96.db, 96x96\n" );
		//	}
		//	else if ( dh.type == 0x04 )
		//	{
		//		printf( "Cache type: thumbcache_256.db, 256x256\n" );
		//	}
		//	else if ( dh.type == 0x05 )
		//	{
		//		printf( "Cache type: thumbcache_1024.db, 1024x1024\n" );
		//	}
		//	else if ( dh.type == 0x06 )
		//	{
		//		printf( "Cache type: thumbcache_1600.db, 1600x1600\n" );
		//	}
		//	else if ( dh.type == 0x07 )
		//	{
		//		printf( "Cache type: thumbcache_sr.db\n" );
		//	}
		//	else if ( dh.type == 0x08 )
		//	{
		//		printf( "Cache type: thumbcache_wide.db\n" );
		//	}
		//	else if ( dh.type == 0x09 )
		//	{
		//		printf( "Cache type: thumbcache_exif.db\n" );
		//	}
		//	else if ( dh.type == 0x0A )
		//	{
		//		printf( "Cache type: thumbcache_wide_alternate.db\n" );
		//	}
		//	else
		//	{
		//		printf( "Cache type: Unknown\n" );
		//	}
		//}
		//else // Windows 8: 00 = 16, 01 = 32, 02 = 48, 03 = 96, 04 = 256, 05 = 1024, 06 = sr, 07 = wide, 08 = exif
		//{
		//	if ( dh.type == 0x00 )
		//	{
		//		printf( "Cache type: thumbcache_16.db, 16x16\n" );
		//	}
		//	else if ( dh.type == 0x01 )
		//	{
		//		printf( "Cache type: thumbcache_32.db, 32x32\n" );
		//	}
		//	else if ( dh.type == 0x02 )
		//	{
		//		printf( "Cache type: thumbcache_48.db, 48x48\n" );
		//	}
		//	else if ( dh.type == 0x03 )
		//	{
		//		printf( "Cache type: thumbcache_96.db, 96x96\n" );
		//	}
		//	else if ( dh.type == 0x04 )
		//	{
		//		printf( "Cache type: thumbcache_256.db, 256x256\n" );
		//	}
		//	else if ( dh.type == 0x05 )
		//	{
		//		printf( "Cache type: thumbcache_1024.db, 1024x1024\n" );
		//	}
		//	else if ( dh.type == 0x06 )
		//	{
		//		printf( "Cache type: thumbcache_sr.db\n" );
		//	}
		//	else if ( dh.type == 0x07 )
		//	{
		//		printf( "Cache type: thumbcache_wide.db\n" );
		//	}
		//	else if ( dh.type == 0x08 )
		//	{
		//		printf( "Cache type: thumbcache_exif.db\n" );
		//	}
		//	else
		//	{
		//		printf( "Cache type: Unknown\n" );
		//	}
		//}

		unsigned int first_cache_entry = 0;
		unsigned int available_cache_entry = 0;
		unsigned int number_of_cache_entries = 0;

		// WINDOWS_8v2 has an additional 4 bytes before the entry information.
		if ( dh.version == WINDOWS_8v2 )
		{
			database_header_entry_info_v2 dhei = { 0 };
			ReadFile( hFile, &dhei, sizeof( database_header_entry_info_v2 ), &read, NULL );
			first_cache_entry = dhei.first_cache_entry;
			available_cache_entry = dhei.available_cache_entry;
			number_of_cache_entries = dhei.number_of_cache_entries;
		}
		else if ( dh.version == WINDOWS_8v3 || dh.version == WINDOWS_8_1 || dh.version == WINDOWS_10 )
		{
			database_header_entry_info_v3 dhei = { 0 };
			ReadFile( hFile, &dhei, sizeof( database_header_entry_info_v3 ), &read, NULL );
			first_cache_entry = dhei.first_cache_entry;
			available_cache_entry = dhei.available_cache_entry;
		}
		else
		{
			database_header_entry_info dhei = { 0 };
			ReadFile( hFile, &dhei, sizeof( database_header_entry_info ), &read, NULL );
			first_cache_entry = dhei.first_cache_entry;
			available_cache_entry = dhei.available_cache_entry;
			number_of_cache_entries = dhei.number_of_cache_entries;
		}

		// Offset to the first cache entry.
		//printf( "Offset to first cache entry: %lu bytes\n", first_cache_entry );

		// Offset to the available cache entry.
		//printf( "Offset to available cache entry: %lu bytes\n", available_cache_entry );

		// Number of cache entries.
		if ( dh.version != WINDOWS_8v3 && dh.version != WINDOWS_8_1 && dh.version != WINDOWS_10 )
		{
			//printf( "Number of cache entries: %lu\n", number_of_cache_entries );
		}
		else
		{
			//printf( "Number of cache entries: Unknown\n" );
		}

		//printf( "---------------------------------------------\n" );

		// Set the file pointer to the first possible cache entry. (Should be at an offset equal to the size of the header)
		unsigned int current_position = ( dh.version != WINDOWS_8v2 ? 24 : 28 );

		// Create and set the directory that we'll be outputting files to.
		if ( GetFileAttributes( output_path ) == INVALID_FILE_ATTRIBUTES )
		{
			CreateDirectory( output_path, NULL );
		}

		SetCurrentDirectory( output_path );				// Set the path (relative or full)
		GetCurrentDirectory( MAX_PATH, output_path );	// Get the full path

		// Convert our wide character strings to UTF-8 if we're going to output a report.
		if ( output_html || output_csv )
		{
			utf8_path_length = WideCharToMultiByte( CP_UTF8, 0, output_path, -1, NULL, 0, NULL, NULL );
			utf8_path = ( char * )malloc( sizeof( char ) * utf8_path_length );
			WideCharToMultiByte( CP_UTF8, 0, output_path, -1, utf8_path, utf8_path_length, NULL, NULL );

			utf8_name_length = WideCharToMultiByte( CP_UTF8, 0, dbname, -1, NULL, 0, NULL, NULL );
			utf8_name = ( char * )malloc( sizeof( char ) * utf8_name_length );
			WideCharToMultiByte( CP_UTF8, 0, dbname, -1, utf8_name, utf8_name_length, NULL, NULL );
		}



		// Free our UTF-8 strings.
		free( utf8_name );
		free( utf8_path );

		// Go through our database and attempt to extract each cache entry.
		for ( unsigned int i = 0; true; ++i )
		{
			//printf( "\n---------------------------------------------\n" );
			//printf( "Extracting cache entry %lu at %lu bytes.\n", i + 1, current_position );
			//printf( "---------------------------------------------\n" );

			file_offset = current_position;	// Save for our report files.

			// Set the file pointer to the end of the last cache entry.
			current_position = SetFilePointer( hFile, current_position, NULL, FILE_BEGIN );
			if ( current_position == INVALID_SET_FILE_POINTER )
			{
				//printf( "End of file reached. There are no more entries.\n" );
				break;
			}

			void *database_cache_entry = NULL;
			
			// Determine the type of database we're working with and store its content in the correct structure.
			if ( dh.version == WINDOWS_7 )
			{
				database_cache_entry = ( database_cache_entry_7 * )malloc( sizeof( database_cache_entry_7 ) );
				ReadFile( hFile, database_cache_entry, sizeof( database_cache_entry_7 ), &read, NULL );
				
				// Make sure it's a thumbcache database and the structure was filled correctly.
				if ( read != sizeof( database_cache_entry_7 ) )
				{
					free( database_cache_entry );
					//printf( "End of file reached. There are no more entries.\n" );
					break;
				}
				else if ( memcmp( ( ( database_cache_entry_7 * )database_cache_entry )->magic_identifier, "CMMM", 4 ) != 0 )
				{
					free( database_cache_entry );

					//printf( "Invalid cache entry located at %lu bytes.\n", current_position );
					//printf( "Attempting to scan for next entry.\n" );

					// Walk back to the end of the last cache entry.
					current_position = SetFilePointer( hFile, current_position, NULL, FILE_BEGIN );

					// If we found the beginning of the entry, attempt to read it again.
					if ( scan_memory( hFile, current_position ) )
					{
						//printf( "A valid entry has been found.\n" );
						//printf( "---------------------------------------------\n" );
						--i;
						continue;
					}

					//printf( "Scan failed to find any valid entries.\n" );
					//printf( "---------------------------------------------\n" );
					break;
				}
			}
			else if ( dh.version == WINDOWS_VISTA )
			{
				database_cache_entry = ( database_cache_entry_vista * )malloc( sizeof( database_cache_entry_vista ) );
				ReadFile( hFile, database_cache_entry, sizeof( database_cache_entry_vista ), &read, NULL );
				
				// Make sure it's a thumbcache database and the structure was filled correctly.
				if ( read != sizeof( database_cache_entry_vista ) )
				{
					free( database_cache_entry );
					//printf( "End of file reached. There are no more entries.\n" );
					break;
				}
				else if ( memcmp( ( ( database_cache_entry_vista * )database_cache_entry )->magic_identifier, "CMMM", 4 ) != 0 )
				{
					free( database_cache_entry );

					//printf( "Invalid cache entry located at %lu bytes.\n", current_position );
					//printf( "Attempting to scan for next entry.\n" );

					// Walk back to the end of the last cache entry.
					current_position = SetFilePointer( hFile, current_position, NULL, FILE_BEGIN );

					// If we found the beginning of the entry, attempt to read it again.
					if ( scan_memory( hFile, current_position ) )
					{
						//printf( "A valid entry has been found.\n" );
						//printf( "---------------------------------------------\n" );
						--i;
						continue;
					}

					//printf( "Scan failed to find any valid entries.\n" );
					//printf( "---------------------------------------------\n" );
					break;
				}
			}
			else if ( dh.version == WINDOWS_8 || dh.version == WINDOWS_8v2 || dh.version == WINDOWS_8v3 || dh.version == WINDOWS_8_1 || dh.version == WINDOWS_10 )
			{
				database_cache_entry = ( database_cache_entry_8 * )malloc( sizeof( database_cache_entry_8 ) );
				ReadFile( hFile, database_cache_entry, sizeof( database_cache_entry_8 ), &read, NULL );
				
				// Make sure it's a thumbcache database and the structure was filled correctly.
				if ( read != sizeof( database_cache_entry_8 ) )
				{
					free( database_cache_entry );
					//printf( "End of file reached. There are no more entries.\n" );
					break;
				}
				else if ( memcmp( ( ( database_cache_entry_8 * )database_cache_entry )->magic_identifier, "CMMM", 4 ) != 0 )
				{
					free( database_cache_entry );

					//printf( "Invalid cache entry located at %lu bytes.\n", current_position );
					//printf( "Attempting to scan for next entry.\n" );

					// Walk back to the end of the last cache entry.
					current_position = SetFilePointer( hFile, current_position, NULL, FILE_BEGIN );

					// If we found the beginning of the entry, attempt to read it again.
					if ( scan_memory( hFile, current_position ) )
					{
						//printf( "A valid entry has been found.\n" );
						//printf( "---------------------------------------------\n" );
						--i;
						continue;
					}

					//printf( "Scan failed to find any valid entries.\n" );
					//printf( "---------------------------------------------\n" );
					break;
				}
			}

			// I think this signifies the end of a valid database and everything beyond this is data that's been overwritten.
			if ( ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->entry_hash : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->entry_hash : ( ( database_cache_entry_8 * )database_cache_entry )->entry_hash ) ) == 0 )
			{
				//printf( "Empty cache entry located at %lu bytes.\n", current_position );
				//printf( "Adjusting offset for next entry.\n" );
				//printf( "---------------------------------------------\n" );

				// Skip the header of this entry. If the next position is invalid (which it probably will be), we'll end up scanning.
				current_position += read;
				--i;
				// Free each database entry that we've skipped over.
				free( database_cache_entry );

				continue;
			}

			// Cache size includes the 4 byte signature and itself ( 4 bytes ).
			unsigned int cache_entry_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->cache_entry_size : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->cache_entry_size : ( ( database_cache_entry_8 * )database_cache_entry )->cache_entry_size ) );

			current_position += cache_entry_size;

			// The magic identifier for the current entry.
			char *magic_identifier = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->magic_identifier : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->magic_identifier : ( ( database_cache_entry_8 * )database_cache_entry )->magic_identifier ) );
			memcpy( stmp, magic_identifier, sizeof( char ) * 4 );
			//printf( "Signature (magic identifier): %s\n", stmp );

			//printf( "Cache size: %lu bytes\n", cache_entry_size );

			// The entry hash may be the same as the filename.
			char s_entry_hash[ 19 ] = { 0 };
			sprintf_s( s_entry_hash, 19, "0x%016llx", ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->entry_hash : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->entry_hash : ( ( database_cache_entry_8 * )database_cache_entry )->entry_hash ) ) );	// This will probably be the same as the file name.
			//printf_s( "Entry hash: %s\n", s_entry_hash );

			// Windows Vista
			if ( dh.version == WINDOWS_VISTA )
			{
				// UTF-16 file extension.
				//wprintf_s( L"File extension: %.4s\n", ( ( database_cache_entry_vista * )database_cache_entry )->extension );
			}

			// The length of our filename.
			unsigned int filename_length = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->filename_length : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->filename_length : ( ( database_cache_entry_8 * )database_cache_entry )->filename_length ) );
			//printf( "Identifier string size: %lu bytes\n", filename_length );

			// Padding size.
			unsigned int padding_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->padding_size : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->padding_size : ( ( database_cache_entry_8 * )database_cache_entry )->padding_size ) );
			//printf( "Padding size: %lu bytes\n", padding_size );

			// The size of our data.
			unsigned int data_size = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->data_size : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->data_size : ( ( database_cache_entry_8 * )database_cache_entry )->data_size ) );
			//printf( "Data size: %lu bytes\n", data_size );

			// Windows 8/8.1/10 contains the width and height of the image.
			if ( dh.version == WINDOWS_8 || dh.version == WINDOWS_8v2 || dh.version == WINDOWS_8v3 || dh.version == WINDOWS_8_1 || dh.version == WINDOWS_10 )
			{
				//printf( "Dimensions: %lux%lu\n", ( ( database_cache_entry_8 * )database_cache_entry )->width, ( ( database_cache_entry_8 * )database_cache_entry )->height );
			}

			// Unknown value.
			unsigned int unknown = ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->unknown : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->unknown : ( ( database_cache_entry_8 * )database_cache_entry )->unknown ) );
			//printf( "Unknown value: 0x%04x\n", unknown );

			// CRC-64 data checksum.
			char s_data_checksum[ 19 ] = { 0 };
			sprintf_s( s_data_checksum, 19, "0x%016llx", ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->data_checksum : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->data_checksum : ( ( database_cache_entry_8 * )database_cache_entry )->data_checksum ) ) );
			//printf_s( "Data checksum (CRC-64): %s\n", s_data_checksum );

			// CRC-64 header checksum.
			char s_header_checksum[ 19 ] = { 0 };
			sprintf_s( s_header_checksum, 19, "0x%016llx", ( ( dh.version == WINDOWS_7 ) ? ( ( database_cache_entry_7 * )database_cache_entry )->header_checksum : ( ( dh.version == WINDOWS_VISTA ) ? ( ( database_cache_entry_vista * )database_cache_entry )->header_checksum : ( ( database_cache_entry_8 * )database_cache_entry )->header_checksum ) ) );
			//printf_s( "Header checksum (CRC-64): %s\n", s_header_checksum );

			// Since the database can store CLSIDs that extend beyond MAX_PATH, we'll have to set a larger truncation length. A length of 32767 would probably never be seen. 
			unsigned int filename_truncate_length = min( filename_length, ( sizeof( wchar_t ) * SHRT_MAX ) );

			// UTF-16 filename. Allocate the filename length plus 6 for the unicode extension and null character.
			wchar_t *filename = ( wchar_t * )malloc( filename_truncate_length + ( sizeof( wchar_t ) * 6 ) );
			memset( filename, 0, filename_truncate_length + ( sizeof( wchar_t ) * 6 ) );
			ReadFile( hFile, filename, filename_truncate_length, &read, NULL );
			if ( read == 0 )
			{
				free( filename );
				free( database_cache_entry );
				//printf( "End of file reached. There are no more valid entries.\n" );
				break;
			}

			unsigned int file_position = 0;

			// Adjust our file pointer if we truncated the filename. This really shouldn't happen unless someone tampered with the database, or it became corrupt.
			if ( filename_length > filename_truncate_length )
			{
				// Offset the file pointer and see if we've moved beyond the EOF.
				file_position = SetFilePointer( hFile, filename_length - filename_truncate_length, 0, FILE_CURRENT );
				if ( file_position == INVALID_SET_FILE_POINTER )
				{
					free( filename );
					free( database_cache_entry );
					//printf( "End of file reached. There are no more valid entries.\n" );
					break;
				}
			}

			// This will set our file pointer to the beginning of the data entry.
			file_position = SetFilePointer( hFile, padding_size, 0, FILE_CURRENT );
			if ( file_position == INVALID_SET_FILE_POINTER )
			{
				free( filename );
				free( database_cache_entry );
				//printf( "End of file reached. There are no more valid entries.\n" );
				break;
			}

			// Retrieve the data content.
			char *buf = NULL;
			
			if ( data_size != 0 )
			{
				buf = ( char * )malloc( sizeof( char ) * data_size );
				ReadFile( hFile, buf, data_size, &read, NULL );
				if ( read == 0 )
				{
					free( buf );
					free( filename );
					free( database_cache_entry );
					//printf( "End of file reached. There are no more valid entries.\n" );
					break;
				}

				// Detect the file extension and copy it into the filename string.
				if ( memcmp( buf, FILE_TYPE_BMP, 2 ) == 0 )			// First 3 bytes
				{
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 4, L".bmp", 4 );
				}
				else if ( memcmp( buf, FILE_TYPE_JPEG, 4 ) == 0 )	// First 4 bytes
				{
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 4, L".jpg", 4 );
				}
				else if ( memcmp( buf, FILE_TYPE_PNG, 8 ) == 0 )	// First 8 bytes
				{
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 4, L".png", 4 );
				}
				else if ( dh.version == WINDOWS_VISTA && ( ( database_cache_entry_vista * )database_cache_entry )->extension[ 0 ] != NULL )	// If it's a Windows Vista thumbcache file and we can't detect the extension, then use the one given.
				{
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 1, L".", 1 );
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ) + 1, 4, ( ( database_cache_entry_vista * )database_cache_entry )->extension, 4 );
				}
			}
			else
			{
				// Windows Vista thumbcache files should include the extension.
				if ( dh.version == WINDOWS_VISTA && ( ( database_cache_entry_vista * )database_cache_entry )->extension[ 0 ] != NULL )
				{
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ), 1, L".", 1 );
					wmemcpy_s( filename + ( filename_truncate_length / sizeof( wchar_t ) ) + 1, 4, ( ( database_cache_entry_vista * )database_cache_entry )->extension, 4 ); 
				}
			}

			//wprintf_s( L"Identifier string: %s\n", filename );

			char *utf8_filename = NULL;
			int utf8_filename_length = 0;

			// Write the entry to a new line in the CSV report file.
			if ( output_csv && ( !skip_blank || ( skip_blank && data_size > 0 ) ) )
			{
				char buf[ 125 ];
				int write_size = 0;
				if ( dh.version == WINDOWS_8 || dh.version == WINDOWS_8v2 || dh.version == WINDOWS_8v3 || dh.version == WINDOWS_8_1 || dh.version == WINDOWS_10 )	// Windows 8/8.1/10 includes dimensions (width x height)
				{
					write_size = sprintf_s( buf, 125, "%lu,%lu,%lu,%lu,%lux%lu,%s,%s,%s,\"", i + 1, file_offset, cache_entry_size, data_size, ( ( database_cache_entry_8 * )database_cache_entry )->width, ( ( database_cache_entry_8 * )database_cache_entry )->height, s_entry_hash, s_data_checksum, s_header_checksum );
				}
				else
				{
					write_size = sprintf_s( buf, 125, "%lu,%lu,%lu,%lu,%s,%s,%s,\"", i + 1, file_offset, cache_entry_size, data_size, s_entry_hash, s_data_checksum, s_header_checksum );
				}
				WriteFile( hFile_csv, buf, write_size, &written, NULL );

				utf8_filename_length = WideCharToMultiByte( CP_UTF8, 0, filename, -1, NULL, 0, NULL, NULL );
				utf8_filename = ( char * )malloc( sizeof( char ) * utf8_filename_length );	// Includes NULL character.
				WideCharToMultiByte( CP_UTF8, 0, filename, -1, utf8_filename, utf8_filename_length, NULL, NULL );

				char *out_buf = ( char * )malloc( sizeof( char ) * ( utf8_filename_length + 3 ) );
				write_size = sprintf_s( out_buf, utf8_filename_length + 3, "%s\"\r\n", utf8_filename );
				WriteFile( hFile_csv, out_buf, write_size, &written, NULL );

				free( out_buf );

				// Save the filename if we're going to output an html file. Cuts down on the number of conversions.
				if ( !output_html )
				{
					free( utf8_filename );
				}
			}

			// Write the entry to a new table row in the HTML report file.
			if ( output_html && ( !skip_blank || ( skip_blank && data_size > 0 ) ) )
			{
				char buf[ 196 ];
				int write_size = 0;
				if ( dh.version == WINDOWS_8 || dh.version == WINDOWS_8v2 || dh.version == WINDOWS_8v3 || dh.version == WINDOWS_8_1 || dh.version == WINDOWS_10 )	// Windows 8/8.1/10 includes dimensions (width x height)
				{
					write_size = sprintf_s( buf, 196, "<tr><td>%lu</td><td>%lu</td><td>%lu</td><td>%lu</td><td>%lux%lu</td><td>%s</td><td>%s</td><td>%s</td><td>", i + 1, file_offset, cache_entry_size, data_size, ( ( database_cache_entry_8 * )database_cache_entry )->width, ( ( database_cache_entry_8 * )database_cache_entry )->height, s_entry_hash, s_data_checksum, s_header_checksum );
				}
				else
				{
					write_size = sprintf_s( buf, 196, "<tr><td>%lu</td><td>%lu</td><td>%lu</td><td>%lu</td><td>%s</td><td>%s</td><td>%s</td><td>", i + 1, file_offset, cache_entry_size, data_size, s_entry_hash, s_data_checksum, s_header_checksum );
				}
				WriteFile( hFile_html, buf, write_size, &written, NULL );

				if ( utf8_filename == NULL )
				{
					utf8_filename_length = WideCharToMultiByte( CP_UTF8, 0, filename, -1, NULL, 0, NULL, NULL );
					utf8_filename = ( char * )malloc( sizeof( char ) * utf8_filename_length );	// Includes NULL character.
					WideCharToMultiByte( CP_UTF8, 0, filename, -1, utf8_filename, utf8_filename_length, NULL, NULL );
				}
				WriteFile( hFile_html, utf8_filename, utf8_filename_length - 1, &written, NULL );

				// If there's an image we want to extract, then insert it into the last column.
				if ( data_size != 0 && extract_thumbnails )
				{
					// Replace any invalid filename characters with an underscore "_".
					char *filename_ptr = utf8_filename;
					while( filename_ptr != NULL && *filename_ptr != NULL )
					{
						if ( *filename_ptr == '\\' ||
							 *filename_ptr == '/' ||
							 *filename_ptr == ':' ||
							 *filename_ptr == '*' ||
							 *filename_ptr == '?' ||
							 *filename_ptr == '\"' ||
							 *filename_ptr == '<' ||
							 *filename_ptr == '>' ||
							 *filename_ptr == '|' )
						{
							*filename_ptr = '_';
						}

						++filename_ptr;
					}

					char *out_buf = ( char * )malloc( sizeof( char ) * ( utf8_filename_length + 33 ) );
					write_size = sprintf_s( out_buf, utf8_filename_length + 33, "</td><td><img src=\"%s\" /></td></tr>", utf8_filename );

					WriteFile( hFile_html, out_buf, write_size, &written, NULL );

					free( out_buf );
				}
				else	// Otherwise, the column will remain empty.
				{
					WriteFile( hFile_html, "</td><td></td></tr>", 19, &written, NULL );
				}

				free( utf8_filename );
			}

			// Check if the cache entry is in the list of IDs to output.
			std::string lookupID(s_entry_hash, 18); // Cache IDs are 8 bytes / 16 hex characters + 0x
			auto search = ids.find(lookupID);
			if (search != ids.end()) {
				// found in ID map, save the file.


				// Output the data with the given (UTF-16) filename.
				//printf("---------------------------------------------\n");
				if (data_size != 0 && extract_thumbnails)
				{

					// Attempt to save the buffer to a file.
					std::wstring fname = nameidmap[lookupID];
					const wchar_t* wft= fname.c_str();
					HANDLE hFile_save = CreateFile(wft, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
					if (hFile_save != INVALID_HANDLE_VALUE)
					{
						WriteFile(hFile_save, buf, data_size, &written, NULL);
						CloseHandle(hFile_save);
						//printf("Writing complete.\n");
						ids.erase(lookupID); // Clear this ID, as it's saved now.

					}
					else
					{
						//printf("Writing failed.\n");
					}
				}
			}


			else if ( !extract_thumbnails )
			{
				//printf( "Writing skipped.\n" );
			}
			else
			{
				//printf( "No data to write.\n" );
			}
			//printf( "---------------------------------------------\n" );

			// Delete our data buffer.
			free( buf );

			// Delete our filename.
			free( filename );

			// Delete our database cache entry.
			free( database_cache_entry );
		}

		// Close our HTML report.
		if ( output_html )
		{
			WriteFile( hFile_html, "</table><br /></body></html>", 28, &written, NULL );
			CloseHandle( hFile_html );
		}

		// Close our CSV report.
		if ( output_csv )
		{
			WriteFile( hFile_csv, "\r\n", 2, &written, NULL );
			CloseHandle( hFile_csv );
		}

		// Close the input file.
		CloseHandle( hFile );
	}
	else
	{
		// See if they typed an incorrect filename.
		if ( GetLastError() == ERROR_FILE_NOT_FOUND )
		{
			printf( "The thumbcache database does not exist.\n" );
		}
		else	// For all other errors, it probably failed to open.
		{
			printf( "The thumbcache database failed to open.\n" );
		}
	}

	return ids;
}
