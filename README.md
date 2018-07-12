# shellthumbs Repo
Windows 10 thumbnail extraction using thumbcache

## shellthumbs

Use the Windows Shell API to extract thumbnails of size <thumbsize> from images in <folderpath>.
Images are saved to the current directory in the bitmap format (JPEG also available if it's uncommented below).
Output files do not possess binary identity to those in the thumbcache.


## shellthumbsfromcache

An application to create Windows thumbnails for a given directory and output them to a file. Output files retain their original name, even if the file type changes (i.e. from jpeg to bitmap for the 96x96 thumbnails).
Items are cached and then retrieved directly from the thumbcache such that binary identity with actual thumbcache entries is preserved.

As parsing the cache every time an image is thumbnailed is expensive, thumbnails are recovered in batches from the thumbcache. This can be controlled by changing the <batchsize> argument.
Note that if batch size is larger than the number of items in the target directory, nothing will be saved. Make sure that all items are processed by selecting batch size such that:
num_files_in_directory % batchsize == 0


### ThumbcacheByID

Modified code from thumbcache_viewer_cmd (https://github.com/thumbcacheviewer/thumbcacheviewer) to wrap parsing in a function which only extracts thumbnails which are passed in an ID map. Changes largely involve commenting out print statements and using maps to keep track of which cache entries (IDs) should be saved to an output directory.



## benchmarkLookip

An application to call lookupThumbcache in order to benchmark the time it takes to parse the thumbcache for particular items.

## CRC64Data
An application to wrap the CRC64 checksum implementation from  thumbcache_viewer_cmd (https://github.com/thumbcacheviewer/thumbcacheviewer).
If these checksums are applied to thumbnails extracted from the thumbcache, they may be used to lookup the entry in the cache itself at a later time.


## Utils

Contains python utilities for generating dummy CRC and SHA256 lists, as well as batch scripts for executing the performance benchmarks multiple times.



## Build

Open the shellthumbs.sln solution. You can then build the solution or a given projact for Windows 10 or 8.1 and lower.


# Notes
If running via Windows Powershell, paths need to use double backslashes.
i.e.: C:\\Users\\USERNAME\\....
