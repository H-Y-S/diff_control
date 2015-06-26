# example helloworld.py

import pygtk
pygtk.require('2.0')
import gtk
import string
import datetime
from ConfigParser import SafeConfigParser

import mountpoint_conversion

NUM_SIGNED_FLOAT_FIELD_NAMES = ['rot_start_entry','rot_end_entry']

NUM_FLOAT_FIELD_NAMES = ['acq_time_entry',
                        'z_range_entry','z_center_entry']

NUM_INTEGER_FIELD_NAMES = ['rot_step_entry','z_step_entry',
                        'acq_img_number_entry']

FILENAME_CHARS = "-_%s%s" % (string.ascii_letters,string.digits)
CONFIG_FILE_NAME = 'config.ini'


class DiffControl:
    # This one is called when the main window close-button is clicked
    def on_main_window_delete_event(self, widget, event, data=None):
        # If you return FALSE in the "delete_event" signal handler,
        # GTK will emit the "destroy" signal. Returning TRUE means
        # you don't want the window to be destroyed.
        # This is useful for popping up 'are you sure you want to quit?'
        # type dialogs.
        print "delete event occurred"

        # Change FALSE to TRUE and the main window will not be destroyed
        # with a "delete_event".
        # TODO: Confirm close
        return False

    # This one is called when the main window is destroyed (i.e. when  
    # delete_event returns null)
    def on_main_window_destroy(self, widget, data=None):
        print "destroy signal occurred"
        self.write_config_file() # write the changed configurations
        gtk.main_quit()


    # Constructor
    def __init__(self):
        self.builder = gtk.Builder()
        self.builder.add_from_file("diff_control.glade")
        self.init_values() # init values before connecting the fields

        self.builder.connect_signals(self)
        
        
        for i in NUM_SIGNED_FLOAT_FIELD_NAMES:
            entry = self.builder.get_object(i)
            self.numerify_float_signed(entry)

        for i in NUM_FLOAT_FIELD_NAMES:
            entry = self.builder.get_object(i)
            self.numerify_float(entry)

        for i in NUM_INTEGER_FIELD_NAMES:
            entry = self.builder.get_object(i)
            self.numerify_integer(entry)

        entry = self.builder.get_object('acq_filename_entry')
        self.constrain_filename(entry)
        

    # start the event-loop (and end of control here)
    def run(self):
        self.builder.get_object("main_window").show_all()
        gtk.main()


    # This connects text fields to method that keeps the input numerically valid
    def numerify_float_signed(self,widget):
        def filter_numbers(entry, *args):
            text = entry.get_text().strip()
            text = ''.join([i for i in text if i in '+-.0123456789'])
            if len(text) > 0:
                text = text[0] + ''.join([i for i in text[1:] if i in '.0123456789']) # +/- only at the beginning

                ind = text.find('.')
                if ind > 0:
                    text = text[:ind+1] + ''.join([i for i in text[ind+1:] if i in '0123456789'])
                
            entry.set_text(text)

        widget.connect('changed', filter_numbers)
        widget.connect('changed', self.get_values_from_controls)       
       
        
    # This connects text fields to method that keeps the input numerically valid
    def numerify_float(self,widget):
        def filter_numbers(entry, *args):
            text = entry.get_text().strip()
            text = ''.join([i for i in text if i in '.0123456789'])
            if len(text) > 0:
                ind = text.find('.')
                if ind > 0:
                    text = text[:ind+1] + ''.join([i for i in text[ind+1:] if i in '0123456789'])
                
            entry.set_text(text)

        widget.connect('changed', filter_numbers)       
        widget.connect('changed', self.get_values_from_controls)       


    # This connects text fields to method that keeps the input numerically valid
    def numerify_integer(self,widget):
        def filter_numbers(entry, *args):
            text = entry.get_text().strip()
            text = ''.join([i for i in text if i in '0123456789'])
            entry.set_text(text)

        widget.connect('changed', filter_numbers)
        widget.connect('changed', self.get_values_from_controls)       

    # This connects text field that keeps it a valid filename with no spaces
    def constrain_filename(self,widget):
        def filter_filenames(entry, *args):
            text = entry.get_text().strip()
            text = ''.join([i for i in text if i in FILENAME_CHARS])
            entry.set_text(text)

        widget.connect('changed', filter_filenames)       
        widget.connect('changed', self.get_values_from_controls)       


    def get_values_from_controls(self,widget):
        print('get_values_from_controls')
        text = self.builder.get_object('rot_start_entry').get_text()
        self.mRotStart = self.convert_text_to_float(text)

        text = self.builder.get_object('rot_end_entry').get_text()
        self.mRotEnd = self.convert_text_to_float(text)
            
        text = self.builder.get_object('rot_step_entry').get_text()
        self.mRotStep = self.convert_text_to_float(text)

        text = self.builder.get_object('z_range_entry').get_text()
        self.mZRange = self.convert_text_to_float(text)
            
        text = self.builder.get_object('z_center_entry').get_text()
        self.mZCenter = self.convert_text_to_float(text)
        
        text = self.builder.get_object('z_step_entry').get_text()
        self.mZStep = self.convert_text_to_float(text)

        text = self.builder.get_object('acq_time_entry').get_text()
        self.mAcqTime = self.convert_text_to_float(text)

        text = self.builder.get_object('acq_img_number_entry').get_text()
        self.mAcqCount = self.convert_text_to_float(text)

        self.mFileNamePrefix = self.builder.get_object('acq_filename_entry').get_text()

        self.update_summary_fields()


    def convert_text_to_float(self,text):
        result = 0
        try:
            result = float(text)
        except ValueError:
            result = 0
                        
        return result
        
    def set_values_to_controls(self):
        print('set_values_to_controls')
        self.builder.get_object('rot_start_entry').set_text(str(self.mRotStart))
        self.builder.get_object('rot_end_entry').set_text(str(self.mRotEnd))
        self.builder.get_object('rot_step_entry').set_text(str(self.mRotStep))

        self.builder.get_object('z_center_entry').set_text(str(self.mZCenter))
        self.builder.get_object('z_range_entry').set_text(str(self.mZRange))
        self.builder.get_object('z_step_entry').set_text(str(self.mZStep))

        self.builder.get_object('acq_time_entry').set_text(str(self.mAcqTime))
        self.builder.get_object('acq_img_number_entry').set_text(str(self.mAcqCount))
        self.builder.get_object('acq_filename_entry').set_text(self.mFileNamePrefix)

        self.update_summary_fields()
        self.update_control_buttons()



    def update_summary_fields(self):
        nrot = self.mRotStep
        nz = self.mZStep
    
        
        scantime = nrot*nz*self.mAcqTime*self.mAcqCount
        if scantime < 0:
            scantime = 0            
        self.builder.get_object('estimated_scan_time_display').set_text(str(datetime.timedelta(seconds=scantime)))
        self.builder.get_object('file_name_display').set_text(self.mFileNamePrefix + '####.tif')
        self.builder.get_object('save_location_display').set_text(self.mServerSidePath)
        if self.mSavePathOK:
            print "hiding location warning"
            self.builder.get_object('save_location_warning_image').set_from_pixbuf(None)
            self.builder.get_object('save_location_warning_image').set_tooltip_text(None)
        else:
            print "showing location warning"
            self.builder.get_object('save_location_warning_image').set_from_stock(gtk.STOCK_DIALOG_WARNING,gtk.ICON_SIZE_BUTTON)
            self.builder.get_object('save_location_warning_image').set_tooltip_text("Path not available on the server side. \nSaving images locally to \nPilatus server")
            
        rsize = self.calc_rot_step_size()
        zsize = self.calc_z_step_size()
        self.builder.get_object('rot_step_size').set_text('Step size: ' + '%.2f' % rsize + u'\N{DEGREE SIGN}')
        self.builder.get_object('z_step_size').set_text('Step size: ' + '%.3f' % zsize + ' mm')
        


    def calc_rot_step_size(self):
        rotsize = 0
        if (self.mRotStep > 0):
            rotsize = (abs(self.mRotEnd-self.mRotStart) / self.mRotStep)
 
        return rotsize

        
    def calc_z_step_size(self):
        zsize = 0
        if (self.mZStep > 1):
            zsize = (self.mZRange / (self.mZStep -1))
 
        return zsize

    def init_values(self):
         # Set hard coded defaults
        self.mRotStart = 0
        self.mRotEnd = 180
        self.mRotStep = 18

        self.mZCenter = 200
        self.mZRange = 2
        self.mZStep = 10
        
        self.mAcqTime = 600
        self.mAcqCount = 3

        self.mFileNamePrefix = 'IMG_diffraction'
        self.mScanRunning = False

        # Load from config file values that exist there
        self.read_config_file()
                
        print('Initializing values')
        self.set_values_to_controls()

    
    
    def read_config_file(self):
        # Load from config file values that exist there
        cp = SafeConfigParser()
        cp.read(CONFIG_FILE_NAME)
        
        if cp.has_option('ScanParameters','RotStart'):  
            self.mRotStart = cp.getfloat('ScanParameters','RotStart')
        if cp.has_option('ScanParameters','RotEnd'):  
            self.mRotEnd = cp.getfloat('ScanParameters','RotEnd')
        if cp.has_option('ScanParameters','RotStep'):  
            self.mRotStep = cp.getint('ScanParameters','RotStep')
        if cp.has_option('ScanParameters','ZCenter'):  
            self.mZCenter = cp.getfloat('ScanParameters','ZCenter')
        if cp.has_option('ScanParameters','ZRange'):  
            self.mZRange = cp.getfloat('ScanParameters','ZRange')
        if cp.has_option('ScanParameters','ZStep'):  
            self.mZStep = cp.getint('ScanParameters','ZStep')
        if cp.has_option('ScanParameters','AcqTime'):  
            self.mAcqTime = cp.getfloat('ScanParameters','AcqTime')
        if cp.has_option('ScanParameters','AcqCount'):  
            self.mAcqCount = cp.getfloat('ScanParameters','AcqCount')
        if cp.has_option('ScanParameters','FileNamePrefix'):  
            self.mFileNamePrefix = cp.get('ScanParameters','FileNamePrefix')
        if cp.has_option('ScanParameters','LocalSavePath'):  
            self.mLocalSavePath = cp.get('ScanParameters','LocalSavePath')
            self.mServerSidePath,self.mSavePathOK = mountpoint_conversion.get_pilatus_path(self.mLocalSavePath)
            
    def write_config_file(self):
        cp = SafeConfigParser()
        cp.read(CONFIG_FILE_NAME)

        cp.set('ScanParameters','RotStart','%.6f' % self.mRotStart)
        cp.set('ScanParameters','RotEnd','%.6f' % self.mRotEnd)
        cp.set('ScanParameters','RotStep','%d' % self.mRotStep)
        cp.set('ScanParameters','ZCenter','%.6f' % self.mZCenter)
        cp.set('ScanParameters','ZRange','%.6f' % self.mZRange)
        cp.set('ScanParameters','ZStep','%d' % self.mZStep)
        cp.set('ScanParameters','AcqTime','%.6f' % self.mAcqTime)
        cp.set('ScanParameters','AcqCount','%d' % self.mAcqCount)
        cp.set('ScanParameters','FileNamePrefix',self.mFileNamePrefix)
        cp.set('ScanParameters','LocalSavePath',self.mLocalSavePath)
        
        with open(CONFIG_FILE_NAME, 'wb') as configfile:
            cp.write(configfile)


    def choose_save_location(self, widget, data=None):
        print "choose location clicked"
        chooser = gtk.FileChooserDialog(title=None,action=gtk.FILE_CHOOSER_ACTION_OPEN,
            buttons=(gtk.STOCK_CANCEL,gtk.RESPONSE_CANCEL,gtk.STOCK_OPEN,gtk.RESPONSE_OK)) 
        chooser.set_action(gtk.FILE_CHOOSER_ACTION_SELECT_FOLDER)
        resp = chooser.run()
        chooser.destroy()
        if resp == gtk.RESPONSE_OK:
            self.mLocalSavePath = chooser.get_filename()
            self.mServerSidePath,self.mSavePathOK = mountpoint_conversion.get_pilatus_path(self.mLocalSavePath)
            self.update_summary_fields()
            
            print self.mLocalSavePath, 'selected'
            
        elif resp == gtk.RESPONSE_CANCEL:
            print 'Closed, no files selected'
			

    def start_scan(self, widget, data = None):
        self.mScanRunning = True
        # Set the parameters for the detector
        1
        # Start a thread to do the scanning
    
        # activate the stop scan and deactivate the start scan button
        self.update_control_buttons()
        
    def stop_scan(self, widget, data = None):
        self.mScanRunning = False
        
        # Stop the scan thread
        2
        self.update_control_buttons()
        

    def update_control_buttons(self):
        self.builder.get_object('stop_scan_button').set_sensitive(self.mScanRunning)
        self.builder.get_object('start_scan_button').set_sensitive(not self.mScanRunning)

        


              
# If the program is run directly or passed as an argument to the python
# interpreter then create a HelloWorld instance and show it
if __name__ == "__main__":
    hello = DiffControl()
    hello.run()
