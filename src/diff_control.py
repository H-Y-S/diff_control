# example helloworld.py

import pygtk
pygtk.require('2.0')
import gtk
import string
import datetime
from ConfigParser import SafeConfigParser

import mountpoint_conversion

NUM_SIGNED_FLOAT_FIELD_NAMES = ['mot1_start_entry','mot1_end_entry','mot2_start_entry','mot2_end_entry']

NUM_FLOAT_FIELD_NAMES = ['acq_time_entry']

NUM_INTEGER_FIELD_NAMES = ['mot1_step_entry','mot2_step_entry',
                        'acq_img_number_entry']

FILENAME_CHARS = "-_%s%s" % (string.ascii_letters,string.digits)
CONFIG_FILE_NAME = 'config.ini'

#MOT1_LABEL1 = ['Z Start [mm]','Y Start [mm]','Rot Start [deg]','Z Start [mm]','Rot Start [deg]','Z Start [mm]']
#MOT1_LABEL2 = ['Z End [mm]','Y End [mm]','Rot End [deg]','Z End [mm]','Rot End [deg]','Z End [mm]']
#MOT1_LABEL3 = ['Z Steps','Y Steps','Rot Steps','Z Steps','Rot Steps','Z Steps']


SCAN_TYPE_MOT1_NAMES = ['Z','Y','Rot','Z','Rot','Z']
SCAN_TYPE_MOT2_NAMES = ['','','','Y','Z','Rot']
SCAN_TYPE_MOT1_UNITS = ['mm','mm','deg','mm','deg','mm']
SCAN_TYPE_MOT2_UNITS = ['','','','mm','mm','deg']


class DiffControl:
    # This one is called when the main window is shown
    def on_main_window_show(self, widget):
        # If you return FALSE in the "delete_event" signal handler,
        # GTK will emit the "destroy" signal. Returning TRUE means
        # you don't want the window to be destroyed.
        # This is useful for popping up 'are you sure you want to quit?'
        # type dialogs.
        print "show event occurred"
        self.update_view()


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


    # Constructor, called automatically when an object is created
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
        self.update_view()


        
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

        # Motor 1
        text = self.builder.get_object('mot1_start_entry').get_text()
        self.mMot1Start = self.convert_text_to_float(text)

        text = self.builder.get_object('mot1_end_entry').get_text()
        self.mMot1End = self.convert_text_to_float(text)
            
        text = self.builder.get_object('mot1_step_entry').get_text()
        self.mMot1Step = self.convert_text_to_float(text)

        # Motor 2
        text = self.builder.get_object('mot2_start_entry').get_text()
        self.mMot2Start = self.convert_text_to_float(text)

        text = self.builder.get_object('mot2_end_entry').get_text()
        self.mMot2End = self.convert_text_to_float(text)
            
        text = self.builder.get_object('mot2_step_entry').get_text()
        self.mMot2Step = self.convert_text_to_float(text)


        # Acquisition
        text = self.builder.get_object('acq_time_entry').get_text()
        self.mAcqTime = self.convert_text_to_float(text)

        text = self.builder.get_object('acq_img_number_entry').get_text()
        self.mAcqCount = self.convert_text_to_float(text)

        self.mFileNamePrefix = self.builder.get_object('acq_filename_entry').get_text()

        # update the view
        self.update_view()


    def convert_text_to_float(self,text):
        result = 0
        try:
            result = float(text)
        except ValueError:
            result = 0
                        
        return result

            
    def set_values_to_controls(self):
        print('set_values_to_controls')

        # Motor 1
        self.builder.get_object('mot1_start_entry').set_text(str(self.mMot1Start))
        self.builder.get_object('mot1_end_entry').set_text(str(self.mMot1End))
        self.builder.get_object('mot1_step_entry').set_text(str(self.mMot1Step))

        # Motor 2
        self.builder.get_object('mot2_start_entry').set_text(str(self.mMot2Start))
        self.builder.get_object('mot2_end_entry').set_text(str(self.mMot2End))
        self.builder.get_object('mot2_step_entry').set_text(str(self.mMot2Step))
        
        # Other
        self.builder.get_object('acq_time_entry').set_text(str(self.mAcqTime))
        self.builder.get_object('acq_img_number_entry').set_text(str(self.mAcqCount))
        self.builder.get_object('acq_filename_entry').set_text(self.mFileNamePrefix)

        # Update the view
        self.update_view()



    def set_labels_and_hideshow_fields(self):
        # Set the labels for motors and hide motor2 controls if necessary
        if self.mScanType in [0,1,2] :
            # Hide motor 2 fields
            self.builder.get_object('mot2_label1').hide()
            self.builder.get_object('mot2_label2').hide()
            self.builder.get_object('mot2_label3').hide()
#            self.builder.get_object('mot2_step_size').hide()
            self.builder.get_object('mot2_start_entry').hide()
            self.builder.get_object('mot2_end_entry').hide()
            self.builder.get_object('mot2_step_entry').hide()
            self.builder.get_object('slow_axis_label').hide()
            self.builder.get_object('fast_axis_label').hide()
            
        else :
            # Show motor 2 fields
            self.builder.get_object('mot2_label1').show()
            self.builder.get_object('mot2_label2').show()
            self.builder.get_object('mot2_label3').show()
 #           self.builder.get_object('mot2_step_size').show()
            self.builder.get_object('mot2_start_entry').show()
            self.builder.get_object('mot2_end_entry').show()
            self.builder.get_object('mot2_step_entry').show()
            self.builder.get_object('slow_axis_label').show()
            self.builder.get_object('fast_axis_label').show()


        # Then change the labels
        mot = SCAN_TYPE_MOT1_NAMES[self.mScanType]
        unit = SCAN_TYPE_MOT1_UNITS[self.mScanType]
        self.builder.get_object('mot1_label1').set_text(mot + ' Start [' + unit + ']')
        self.builder.get_object('mot1_label2').set_text(mot + ' End [' + unit + ']')
        self.builder.get_object('mot1_label3').set_text(mot + ' Steps')

        mot = SCAN_TYPE_MOT2_NAMES[self.mScanType]
        unit = SCAN_TYPE_MOT2_UNITS[self.mScanType]
        self.builder.get_object('mot2_label1').set_text(mot + ' Start [' + unit + ']')
        self.builder.get_object('mot2_label2').set_text(mot + ' End [' + unit + ']')
        self.builder.get_object('mot2_label3').set_text(mot + ' Steps')



    # Does all view updates
    def update_view(self):
        self.update_control_buttons()
        self.set_labels_and_hideshow_fields()
        self.update_summary_fields()

                
    def update_summary_fields(self):
        n1 = self.mMot1Step
        n2 = self.mMot2Step
    
        
        scantime = n1*n2*self.mAcqTime*self.mAcqCount
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
            
        mot1size = self.calc_mot1_step_size()
        mot2size = self.calc_mot2_step_size()
#        u'\N{DEGREE SIGN}'
        self.builder.get_object('mot1_step_size').set_text('Step size: ' + '%.2f ' % mot1size + SCAN_TYPE_MOT1_UNITS[self.mScanType])
        if (self.builder.get_object('mot2_start_entry').get_property("visible")) :
            self.builder.get_object('mot2_step_size').set_text('Step size: ' + '%.3f ' % mot2size + SCAN_TYPE_MOT2_UNITS[self.mScanType])
        else :
            self.builder.get_object('mot2_step_size').set_text('')            


    def calc_mot1_step_size(self):
        mot1size = 0
        if (self.mMot1Step > 0):
            mot1size = (abs(self.mMot1End-self.mMot1Start) / self.mMot1Step)
 
        return mot1size

    def calc_mot2_step_size(self):
        mot2size = 0
        if (self.mMot2Step > 0):
            mot2size = (abs(self.mMot2End-self.mMot2Start) / self.mMot2Step)
 
        return mot2size

    def init_values(self):
         # Set hard coded defaults
        self.mMot1MovememntType = 0
        self.mScanType = 0

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

        self.mLocalSavePath = ""
        self.mServerSidePath = ""
        self.mSavePathOK = False
                
        # Load from config file values that exist there
        self.read_config_file()
                
        print('Initializing values')
        self.set_values_to_controls()

    
    
    def read_config_file(self):
        # Load from config file values that exist there
        cp = SafeConfigParser()
        cp.read(CONFIG_FILE_NAME)
        if cp.has_option('ScanParameters','scantype'):  
            self.mScanType = cp.getint('ScanParameters','scantype')
        if cp.has_option('ScanParameters','mot1movementtype'):  
            self.mMot1MovementType = cp.getint('ScanParameters','mot1movementtype')        

        
        # Motor 1        
        if cp.has_option('ScanParameters','mot1start'):  
            self.mMot1Start = cp.getfloat('ScanParameters','mot1start')
        if cp.has_option('ScanParameters','mot1end'):  
            self.mMot1End = cp.getfloat('ScanParameters','mot1end')
        if cp.has_option('ScanParameters','mot1step'):  
            self.mMot1Step = cp.getint('ScanParameters','mot1step')

        # Motor 2
        if cp.has_option('ScanParameters','mot2start'):  
            self.mMot2Start = cp.getfloat('ScanParameters','mot2start')
        if cp.has_option('ScanParameters','mot2end'):  
            self.mMot2End = cp.getfloat('ScanParameters','mot2end')
        if cp.has_option('ScanParameters','mot2step'):  
            self.mMot2Step = cp.getint('ScanParameters','mot2step')

        # Other
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

        # Motor 1
        cp.set('ScanParameters','mot1start','%.6f' % self.mMot1Start)
        cp.set('ScanParameters','mot1end','%.6f' % self.mMot1End)
        cp.set('ScanParameters','mot1step','%d' % self.mMot1Step)

        # Motor 2
        cp.set('ScanParameters','mot2start','%.6f' % self.mMot2Start)
        cp.set('ScanParameters','mot2end','%.6f' % self.mMot2End)
        cp.set('ScanParameters','mot2step','%d' % self.mMot2Step)

        # Other
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
            self.update_view()
            
            print self.mLocalSavePath, 'selected'
            
        elif resp == gtk.RESPONSE_CANCEL:
            print 'Closed, no files selected'
			

    def start_scan(self, widget, data = None):
        self.mScanRunning = True
        # Set the parameters for the detector
        1
        # Start a thread to do the scanning
    
        # activate the stop scan and deactivate the start scan button
        self.update_view()
        
    def stop_scan(self, widget, data = None):
        self.mScanRunning = False
        
        # Stop the scan thread
        2
        self.update_view()
        

    def update_control_buttons(self):
        self.builder.get_object('stop_scan_button').set_sensitive(self.mScanRunning)
        self.builder.get_object('start_scan_button').set_sensitive(not self.mScanRunning)

        
    def movementTypeChanged(self,combobox):
        self.mMovementType = combobox.get_active()
        self.set_labels_and_hideshow_fields()

    def scanTypeChanged(self,combobox):
        print 'scanTypeChanged'
        self.mScanType = combobox.get_active()
        self.update_view()
              
# If the program is run directly or passed as an argument to the python
# interpreter then create a HelloWorld instance and show it
if __name__ == "__main__":
    dc = DiffControl()
    dc.run()
