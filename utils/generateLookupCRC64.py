import os
import csv
import hashlib
from sys import argv
from thread_utils import *


def hashFile(fpath, hashstore):
    #print fpath
    try:
        fil = open(fpath, 'rb')    # binary mode is important
        d=fil.read()
        h = hashlib.sha256(d).hexdigest()
        basename=os.path.basename(fpath)
        hashstore[basename]=h

    except Exception, e:
        print fpath, e

# Parse commandline args.
path = ''

num_threads = 1
if len(argv) == 4:
    path = argv[1]
    num_threads = int(argv[2])
    outfilename = argv[3]
else:
    print "args: <file_directory> <num_threads> <outfilename>"
    exit()


hashes = ThreadSafeDict()
tpool = ThreadPool(num_threads)
counter= 0

# Get file list
flist = []
for folder, subs, files in os.walk(path):
  for filename in files:
    flist.append(os.path.abspath(os.path.join(folder, filename)))

with open(outfilename, "wb") as csvfile:
    for im in flist:
        #fname = os.path.basename(im)
        tpool.add_task(hashFile, im, hashes)
        counter +=1
        if counter % 10000 == 0:
            print "Added {}".format(counter)
    tpool.wait_completion()
    # Write to CSV
    writer = csv.writer(csvfile, delimiter=";")
    writer.writerow(["fname", "SHA256"]) #header row
    for f,h in hashes.iteritems():
        writer.writerow([f,h])
