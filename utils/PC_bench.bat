@echo off
set HDD96=E:\test10k_thumbcache_96.db
set HDD256=E:\test25k_thumbcache_256.db
set SSD96=F:\test10k_thumbcache_96.db
set SSD256=F:\test25k_thumbcache_256.db
set crc_96=.\lookupCRC64_96.txt
set crc_256=.\lookupCRC64_256.txt
set sha256_96=.\lookupSHA256_96.txt
set sha256_256=.\lookupSHA256_256.txt

echo HDD
for /l %%x in (1,1, 25) do (
	.\EmptyStandbyList.exe workingsets
	.\EmptyStandbyList.exe standbylist
	..\bin\benchmarkLookup.exe %HDD96% %sha256_96% %crc_96% >> benchtimes_hdd_96_crcsha.txt
	.\EmptyStandbyList.exe workingsets
	.\EmptyStandbyList.exe standbylist
	..\bin\benchmarkLookup.exe %HDD96% %sha256_96% >> benchtimes_hdd_96_shaonly.txt
	.\EmptyStandbyList.exe workingsets
	.\EmptyStandbyList.exe standbylist
	..\bin\benchmarkLookup.exe %HDD256% %sha256_256% %crc_256% >> benchtimes_hdd_256_crcsha.txt
	.\EmptyStandbyList.exe workingsets
	.\EmptyStandbyList.exe standbylist
	..\bin\benchmarkLookup.exe %HDD256% %sha256_256% >> benchtimes_hdd_256_shaonly.txt
)

echo SSD
for /l %%x in (1,1, 25) do (
	.\EmptyStandbyList.exe workingsets
	.\EmptyStandbyList.exe standbylist
	..\bin\benchmarkLookup.exe %SSD96% %sha256_96% %crc_96% >> benchtimes_ssd_96_crcsha.txt
	.\EmptyStandbyList.exe workingsets
	.\EmptyStandbyList.exe standbylist
	..\bin\benchmarkLookup.exe %SSD96% %sha256_96% >> benchtimes_ssd_96_shaonly.txt
	.\EmptyStandbyList.exe workingsets
	.\EmptyStandbyList.exe standbylist
	..\bin\benchmarkLookup.exe %SSD256% %sha256_256% %crc_256% >> benchtimes_ssd_256_crcsha.txt
	.\EmptyStandbyList.exe workingsets
	.\EmptyStandbyList.exe standbylist
	..\bin\benchmarkLookup.exe %SSD256% %sha256_256% >> benchtimes_ssd_256_shaonly.txt
)
echo done!