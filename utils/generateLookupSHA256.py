import os
import hashlib
import csv
import random
from sys import argv
from thread_utils import *


"""
Uilitiy to generate a file of SHA256s for later lookup. A number of dummy hashes
may be generated in order to inflate the size of the lookup database.
"""




def hashFile(fpath, hashstore):
    """
    Function to generate the SHA256 digest for an input file.
    Hash is stored in the hashtore once generated.
    """
    try:
        fil = open(fpath, 'rb')
        d=fil.read()
        h = hashlib.sha256(d).hexdigest()
        hashstore[h]=1

    except Exception, e:
        print fpath, e


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
    print "args: <file_directory> <num_threads> <numdummyhashes> <outfilename>"
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
print "Hashing files..."
with open(outfilename, "wb") as csvfile:
    for im in flist:
        #fname = os.path.basename(im)
        tpool.add_task(hashFile, im, hashes)
        counter +=1
        if counter % progresscount == 0:
            print "Processed {}".format(counter)

    print "Generating dummy hashes..."
    for i in xrange(0, num_dummies):
        tpool.add_task(genDummyHash, 256, hashes)
        dcounter +=1
        if dcounter % 10000 == 0:
            print "Generated {} dummy hashes.".format(dcounter)
    tpool.wait_completion()
    # Write to CSV
    writer = csv.writer(csvfile, delimiter=";")
    for k,v in hashes.iteritems():
        writer.writerow([k])

print "Done"
