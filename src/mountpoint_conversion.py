import os
import ntpath
import posixpath
import pygtk

pygtk.require('2.0')
import gtk

PILATUS_MOUNTPOINTS = ["/home/det/p2_det/images/","/home/drobo/","/data/01/"]
LOCAL_MOUNTPOINTS = [".","Z:","R:"]

# TODO: This conversion by itself is not enough
# the mountpoint might not be mounted on pilatus
# we have to make sure that the chosen path is available
# and that it stays available during the measurement
# and we should in case of save failure save them locally to 
# pilatus computer somewhere

# converts a windows path to the linux path using the mountpoint mapping
def get_pilatus_path(controlPath):
	# Get the drive
        print "Converting path: " + controlPath
	controlPath = ntpath.normpath(controlPath)
	drv,pth = ntpath.splitdrive(controlPath)
	
	try:
		ind = LOCAL_MOUNTPOINTS.index(drv)	
	except ValueError:
		return PILATUS_MOUNTPOINTS[0], False
	
	# concatenate the pilatus side path
	pil_path = PILATUS_MOUNTPOINTS[ind]+pth
	pil_path = pil_path.replace("\\","/")
	pil_path = posixpath.normpath(pil_path)
        print "Converted path: " + pil_path
	return pil_path, True




def main():
	print get_pilatus_path("Z:/qweqweq\\qweqwe\\\\qweqwe///asass")

	# test for file chooser dialog
	#chooser = gtk.FileChooserDialog(title=None,action=gtk.FILE_CHOOSER_ACTION_OPEN,
		#buttons=(gtk.STOCK_CANCEL,gtk.RESPONSE_CANCEL,gtk.STOCK_OPEN,gtk.RESPONSE_OK))
	#chooser.set_action(gtk.FILE_CHOOSER_ACTION_SELECT_FOLDER)
	#resp = chooser.run()
	#if resp == gtk.RESPONSE_OK:
		#print chooser.get_filename(), 'selected'
	#elif resp == gtk.RESPONSE_CANCEL:
		#print 'Closed, no files selected'

	
if __name__ == '__main__':
    main()


