import os
import hashlib
from sys import argv
from thread_utils import *
from random import getrandbits

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
        basename=os.path.basename(fpath)
        hashstore[basename]=h

    except Exception, e:
        print fpath, e

def genDummyHash(hashlength, hashstore):
    """
    Generate a dummy hex hash.
    """
    print binascii.b2a_hex(os.urandom(15))

    return 1


# Parse commandline args.
path = ''

num_threads = 1
if len(argv) == 4:
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

# Get file list
flist = []
for folder, subs, files in os.walk(path):
  for filename in files:
    flist.append(os.path.abspath(os.path.join(folder, filename)))

print "Generating dummy hashes.."

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
