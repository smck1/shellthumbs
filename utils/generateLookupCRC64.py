import os
import sys
import csv
import random
from sys import argv
import subprocess
from thread_utils import *



PROCESS_NAME = os.path.join(".", "..\\bin\\CRC64Data.exe")
def processFile(impath, hashstore):
    try:
        output = subprocess.check_output([PROCESS_NAME, impath])
        crc64hash = output.splitlines()[0].zfill(64/4) # remove quotes  and new line
        hashstore[crc64hash]=1
    except Exception, e:
        print e

def genDummyHash(hashlength, hashstore):
    """
    Generate a dummy hex hash.
    """
    flag = True
    numhexchars = hashlength/4
    while flag:
        h=hex(random.getrandbits(hashlength))[2:-1]
        if len(h) < numhexchars: # check if it needs padded
            h = h.zfill(numhexchars)
        if not hashstore.has_key(h): # check if it exists
            flag = False
    hashstore[h]=1 # store it



# Parse commandline args.
if len(argv) == 5:
    path = argv[1]
    num_threads = int(argv[2])
    num_dummies = int(argv[3])
    outfilename = argv[4]
else:
    print "args: <file_directory> <num_threads> <numdummychecks> <outfilename>"
    exit()


hashes = ThreadSafeDict()
tpool = ThreadPool(num_threads)
counter= 0
dcounter = 0

# Get file list
flist = []
print "Walking directory for file list..."
for folder, subs, files in os.walk(path):
  for filename in files:
    flist.append(os.path.abspath(os.path.join(folder, filename)))

numfiles = len(flist)
progresscount = numfiles/100
if progresscount <10:
    progresscount = numfiles/2

print "Found {} files.".format(numfiles)
print "Generating CRC64 for files..."
with open(outfilename, "wb") as csvfile:
    for im in flist:
        #fname = os.path.basename(im)
        tpool.add_task(processFile, im, hashes)
        counter +=1
        if counter % progresscount == 0:
            print "Processed {}".format(counter)

    print "Generating dummy checksums..."
    for i in xrange(0, num_dummies):
        tpool.add_task(genDummyHash, 64, hashes)
        dcounter +=1
        if dcounter % 10000 == 0:
            print "Generated {} dummy checksums.".format(dcounter)
    tpool.wait_completion()
    # Write to CSV
    writer = csv.writer(csvfile, delimiter=";")
    for k,v in hashes.iteritems():
        writer.writerow([k])

print "Done"
