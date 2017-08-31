@echo off
set HDD96=E:\test10k_thumbcache_96.db
set HDD256=E:\test25k_thumbcache_256.db
set SSD96=F:\test10k_thumbcache_96.db
set SSD256=F:\test25k_thumbcache_256.db
set crc_96=.\lookupCRC64_96_5mil.txt
set crc_256=.\lookupCRC64_256_5mil.txt
set sha256_96=.\lookupSHA256_96_5mil.txt
set sha256_256=.\lookupSHA256_256_5mil.txt

echo HDD
for /l %%x in (1,1, 10) do (
	echo %%x
	.\EmptyStandbyList.exe workingsets
	.\EmptyStandbyList.exe standbylist
	..\bin\benchmarkLookup.exe %HDD96% %sha256_96% %crc_96% >> benchtimes_hdd_96_crcsha_5mil.txt
	.\EmptyStandbyList.exe workingsets
	.\EmptyStandbyList.exe standbylist
	..\bin\benchmarkLookup.exe %HDD96% %sha256_96% >> benchtimes_hdd_96_shaonly_5mil.txt
	.\EmptyStandbyList.exe workingsets
	.\EmptyStandbyList.exe standbylist
	..\bin\benchmarkLookup.exe %HDD256% %sha256_256% %crc_256% >> benchtimes_hdd_256_crcsha_5mil.txt
	.\EmptyStandbyList.exe workingsets
	.\EmptyStandbyList.exe standbylist
	..\bin\benchmarkLookup.exe %HDD256% %sha256_256% >> benchtimes_hdd_256_shaonly_5mil.txt
)

echo SSD
for /l %%x in (1,1, 10) do (
	echo %%x
	.\EmptyStandbyList.exe workingsets
	.\EmptyStandbyList.exe standbylist
	..\bin\benchmarkLookup.exe %SSD96% %sha256_96% %crc_96% >> benchtimes_ssd_96_crcsha_5mil.txt
	.\EmptyStandbyList.exe workingsets
	.\EmptyStandbyList.exe standbylist
	..\bin\benchmarkLookup.exe %SSD96% %sha256_96% >> benchtimes_ssd_96_shaonly_5mil.txt
	.\EmptyStandbyList.exe workingsets
	.\EmptyStandbyList.exe standbylist
	..\bin\benchmarkLookup.exe %SSD256% %sha256_256% %crc_256% >> benchtimes_ssd_256_crcsha_5mil.txt
	.\EmptyStandbyList.exe workingsets
	.\EmptyStandbyList.exe standbylist
	..\bin\benchmarkLookup.exe %SSD256% %sha256_256% >> benchtimes_ssd_256_shaonly_5mil.txt
)
echo done!