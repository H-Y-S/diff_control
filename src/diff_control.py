import pygtk
pygtk.require('2.0')
import gtk
import gobject
import string
import datetime
import time
import threading
import os
from ConfigParser import SafeConfigParser

import mountpoint_conversion
import can_control
from camera_comm import CameraComm
from motor_axis import MotorAxis

USE_MOTORS = True


# Field IDs for checking correct numeric format
NUM_SIGNED_FLOAT_FIELD_NAMES = ['mot1_start_entry','mot1_end_entry','mot2_start_entry','mot2_end_entry']
NUM_FLOAT_FIELD_NAMES = ['acq_time_entry']
NUM_INTEGER_FIELD_NAMES = ['mot1_step_entry','mot2_step_entry','acq_img_number_entry']

FILENAME_CHARS = "-_%s%s" % (string.ascii_letters,string.digits)
DEFAULT_CONFIG_FILE_NAME = 'config.ini'

SCAN_TYPE_MOT1_NAMES = ['Z','Y','Rot','Z','Rot','Z']
SCAN_TYPE_MOT2_NAMES = ['','','','Y','Z','Rot']
SCAN_TYPE_MOT1_UNITS = ['mm','mm','deg','mm','deg','mm']
SCAN_TYPE_MOT2_UNITS = ['','','','mm','mm','deg']

SINGLE_MOTOR_SCANS = [0,1,2]

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
        self.write_config_file(DEFAULT_CONFIG_FILE_NAME) # write the changed configurations
        self.stop_scan()

        if USE_MOTORS:
            can_control.releasePCAN()
            
        gtk.main_quit()


    # Constructor, called automatically when an object is created
    def __init__(self):

        if USE_MOTORS :
            self.mZAxis = MotorAxis(2)
            self.mYAxis = MotorAxis(1)
            self.mRotAxis = MotorAxis(3)
        self.MOTOR1_MAPPINGS = [self.mZAxis, self.mYAxis, self.mRotAxis, self.mZAxis, self.mRotAxis, self.mZAxis]
        self.MOTOR2_MAPPINGS = [None,None,None,self.mYAxis,self.mZAxis,self.mRotAxis]

        self.builder = gtk.Builder()
        self.builder.add_from_file("diff_control.glade")
        self.init_values() # init values before connecting the fields
        
        self.builder.connect_signals(self)
        self.mScanRunning = False
        
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
        self.mLastLoadedConfigFileName = ''
        
        # if threads do not wor, pygtk may have been compiled without thread support
#        gtk.gdk.threads_init() # Needed for pygtk and threads to work and able to touch GUI
        gobject.threads_init() # Alternatively, threads do not touch GUI




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
        if self.mScanType in SINGLE_MOTOR_SCANS :
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
        self.set_widgets_sensitivity()
        self.set_labels_and_hideshow_fields()
        self.update_summary_fields()



    def set_widgets_sensitivity(self):
        # Set the widgets inactive if scan is running, and active if scan is not running
        self.builder.get_object('menubar1').set_sensitive(not self.mScanRunning)
        self.builder.get_object('hbox2').set_sensitive(not self.mScanRunning)
        self.builder.get_object('frame1').set_sensitive(not self.mScanRunning)
        self.builder.get_object('frame2').set_sensitive(not self.mScanRunning)
        self.builder.get_object('start_scan_button').set_sensitive(not self.mScanRunning)
        self.builder.get_object('stop_scan_button').set_sensitive(self.mScanRunning)
        
    def update_summary_fields(self):
        n1 = self.mMot1Step

        if self.mScanType in SINGLE_MOTOR_SCANS :
            n2 = 1
        else :
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
        if self.mMot1MovementType == 0: # Step-by-step
            if (self.mMot1Step > 0):
                mot1size = (abs(self.mMot1End-self.mMot1Start) / (self.mMot1Step-1))            
        else : # Continuous
            if (self.mMot1Step > 1):
                mot1size = (abs(self.mMot1End-self.mMot1Start) / (self.mMot1Step))                        

        return mot1size

    def calc_mot2_step_size(self):
        mot2size = 0
        if (self.mMot2Step > 1):
            mot2size = (abs(self.mMot2End-self.mMot2Start) / (self.mMot2Step-1))
 
        return mot2size

    def init_values(self):
         # Set hard coded defaults
        self.mMot1MovememntType = 0
        self.mScanType = 0

        self.mMot1Start = 0
        self.mMot1End = 180
        self.mMot1Step = 18

        self.mMot2Start = 0
        self.mMot2End = 180
        self.mMot2Step = 18
        
        self.mAcqTime = 600
        self.mAcqCount = 3

        self.mFileNamePrefix = 'IMG_diffraction'
        self.mScanRunning = False

        self.mLocalSavePath = ""
        self.mServerSidePath = ""
        self.mSavePathOK = False
                
        # Load from default config file values that exist there
        self.read_config_file(DEFAULT_CONFIG_FILE_NAME)
                
        print('Initializing values')
        self.set_values_to_controls()
        self.update_motor_mappings()
    
    
    def read_config_file(self, cfilename):
        # Load from config file values that exist there
        cp = SafeConfigParser()
        cp.read(cfilename)
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
            
    def write_config_file(self, cfilename):
        cp = SafeConfigParser()
        cp.read(cfilename)

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
        
        with open(cfilename, 'wb') as configfile:
            cp.write(configfile)


    def choose_save_location(self, widget, data=None):
        print "choose location clicked"
        chooser = gtk.FileChooserDialog(title=None,action=gtk.FILE_CHOOSER_ACTION_OPEN,
            buttons=(gtk.STOCK_CANCEL,gtk.RESPONSE_CANCEL,gtk.STOCK_OPEN,gtk.RESPONSE_OK)) 
        chooser.set_action(gtk.FILE_CHOOSER_ACTION_SELECT_FOLDER)
        resp = chooser.run()
        if resp == gtk.RESPONSE_OK:
            self.mLocalSavePath = chooser.get_filename()
            self.mServerSidePath,self.mSavePathOK = mountpoint_conversion.get_pilatus_path(self.mLocalSavePath)
            self.update_view()
            
            print self.mLocalSavePath, 'selected'
            
        elif resp == gtk.RESPONSE_CANCEL:
            print 'Closed, no files selected'
			
        chooser.destroy()

    def start_scan(self, widget = None, data = None):
        self.mScanRunning = True
        # Set the parameters for the detector

        # make the scan start from the beginning
        self.mScanPositionIndex = 0
        
        # Start a thread to do the scanning
        self.mScanThread = threading.Thread(target = self.scanThreadEntry)
        self.mScanThread.start()
        
        # activate the stop scan and deactivate the start scan button
        self.update_view()
        
    def stop_scan(self, widget = None, data = None):
        if self.mScanRunning :
            # Stop the scan thread
            self.mScanRunning = False
            self.mScanThread.join()
        
        # update the controls
        self.update_view()
        

        
    def on_movement_type_changed(self,combobox):
        self.mMot1MovementType = combobox.get_active()
        self.update_view()

    def on_scan_type_changed(self,combobox):
        print 'scanTypeChanged'
        self.mScanType = combobox.get_active()
        self.update_view()
        self.update_motor_mappings()


    def update_motor_mappings(self):
        # Update the motor mappings
        self.mMotor1Axis = self.MOTOR1_MAPPINGS[self.mScanType]
        self.mMotor2Axis = self.MOTOR2_MAPPINGS[self.mScanType] 



    def on_open_settings(self, obj) :
        chooser = gtk.FileChooserDialog(title='Open a configuration file',action=gtk.FILE_CHOOSER_ACTION_OPEN,
            buttons=(gtk.STOCK_CANCEL,gtk.RESPONSE_CANCEL,gtk.STOCK_OPEN,gtk.RESPONSE_OK)) 
        resp = chooser.run()
#        chooser.set_current_folder(self.mLastLoadedConfigFileName)
        if resp == gtk.RESPONSE_OK:
            self.mLastLoadedConfigFileName = chooser.get_filename()
            self.read_config_file(self.mLastLoadedConfigFileName)
            self.set_values_to_controls()
            self.update_view()
            
        elif resp == gtk.RESPONSE_CANCEL:
            print 'Closed, no files selected'
        
        chooser.destroy()
        
    def on_save_settings(self, obj) :
        chooser = gtk.FileChooserDialog(title='Open a configuration file',action=gtk.FILE_CHOOSER_ACTION_SAVE,
            buttons=(gtk.STOCK_CANCEL,gtk.RESPONSE_CANCEL,gtk.STOCK_SAVE,gtk.RESPONSE_OK)) 
        chooser.set_current_folder(self.mLastLoadedConfigFileName)
        resp = chooser.run()

        if resp == gtk.RESPONSE_OK:
            self.mLastLoadedConfigFileName = chooser.get_filename()
            self.write_config_file(self.mLastLoadedConfigFileName)
            self.set_values_to_controls()
            self.update_view()
        elif resp == gtk.RESPONSE_CANCEL:
            print 'Closed, no files selected'
        chooser.destroy()

        
    def on_quit_menu_item(self, obj) :
        print 'on_quit_menu_item'
        
        




    def scanThreadEntry(self):
        # Here we have 3 types of activities
        # 1. set new target positions
        # 2. acquire new image
        # 3. wait for movements or images to finish
        #
        # We should be able to stop each activity in case mScanRunning is switched
        # to false (1 second response time is reasonably good)
        #
        # scan ending will be tested with number of images taken
        print 'Starting scan thread'

        # Store the scan parameters locally here
        m1start = self.mMot1Start
        m1end = self.mMot1End
        m1steps = self.mMot1Step
        m1range = m1end-m1start
        m1step = self.calc_mot1_step_size()
        m1name = SCAN_TYPE_MOT1_NAMES[self.mScanType]
        m1unit = SCAN_TYPE_MOT1_UNITS[self.mScanType]

        
        m2start = self.mMot2Start
        m2end = self.mMot2End
        m2steps = self.mMot2Step
        m2range = m2end-m2start
        m2step = self.calc_mot2_step_size()
        m2name = SCAN_TYPE_MOT2_NAMES[self.mScanType]
        m2unit = SCAN_TYPE_MOT2_UNITS[self.mScanType]
        
        is_continuous = self.mMot1MovementType == 1
        is_singleaxis = self.mScanType in SINGLE_MOTOR_SCANS

        if is_singleaxis :
            m2steps = 1
            m2range = 0
            m2step = 0
            

        
            
        # Detector parameters
        acq_time = self.mAcqTime
        acq_N = self.mAcqCount
        filename_prefix = self.mFileNamePrefix

        total_images = m1steps * m2steps * acq_N
                   
        mot2previndex = -1        
        cameraReady = True
        CC = CameraComm("10.110.11.141",41234) # Connect to the camera       
        CC.set_exp_time(acq_time)
        CC.set_exp_period(acq_time + 0.005) # exp period 5 ms longer than exp time is OK
        CC.set_img_path(self.mServerSidePath)
        finishMessage = ''

        # Open a log file
        try:
            logFile = open(os.path.join(self.mLocalSavePath,filename_prefix+'_log.txt'),'w') 
        except IOError :
            print 'Could not open a log file!'


        while (self.mScanRunning) :
            if cameraReady :
                # First write into the log file
                if self.mScanPositionIndex == 0: # write headers
                    if is_singleaxis:
                        logFile.write(m1name + " Start [" + m1unit + "]" + " : " + m1name + " End [" + m1unit + "]" + " : " + "ExposureTime" + " : " "ImageFileName")
                    else:
                        logFile.write(m1name + " Start [" + m1unit + "]" + " : " + m1name + " End [" + m1unit + "]" + " : " + m2name + " Start [" + m2unit + "]" + " : " + m2name + " End [" + m2unit + "]" + " : "+ "ExposureTime" + " : " "ImageFileName")
                else:
                    if is_singleaxis:
                        logFile.write(m1pos_imstart + " : " + m1pos_imend +  " : " + acq_time + " : " + lastfilename )
                    else:
                        logFile.write(m1pos_imstart + " : " + m1pos_imend + " : " + m2pos_imstart+  " : " + m2pos_imend + " : "+ acq_time + " : " + lastfilename)
                
                if self.mScanPositionIndex == total_images:
                    # all points done
                    finishMessage = 'normal finish'
                    break
                
                # Move the motors to new positions if necessary
                mot1index = (self.mScanPositionIndex / acq_N) % (m1steps)
                mot2index = (self.mScanPositionIndex / acq_N) / (m1steps)
                self.mScanPositionIndex += 1
                

                if mot2previndex != mot2index :
                    # move motor 2 and motor 1 to start positions
                    self.mMotor1Axis.moveImmediateSynchronous(m1start)

                    if not is_singleaxis:
                        self.mMotor2Axis.moveImmediateSynchronous(m2start + mot2index*m2step)
                        
                    mot2previndex = mot2index
                    if is_continuous :
                        # This is a new line for motor2, so
                        # start the motor1 (fast axis) continuous movement
                        move_time = ack_time*acq_N*m1steps
                        move_speed = (m1end-m1start) / move_time
                        self.motor1Axis.startMoving(m1end,move_speed)                                

                    
                # set a new position, step by step or continuous
                if not is_continuous :
                    # Step-by-step, move the motor1
                    self.mMotor1Axis.moveImmediateSynchronous(mot1index*m1step + m1start)
                    

                # log the starting positions of motors
                m1pos_imstart = self.mMotor1Axis.getPosition()
                if not is_singleaxis:
                    m2pos_imstart = self.mMotor2Axis.getPosition()
                
                # Start the camera for imaging
                lastfilename = filename_prefix + "%04d" % (self.mScanPositionIndex) + ".tif"
                camOK,camFinished = CC.expose_image(lastfilename)

                if not camOK:
                    print("Exposure does not work") # we should cancel the scan here
                    finishMessage = 'Exposure does not work'
                    break
                elif not camFinished :
                    cameraReady = False
                else:
                    m1pos_imend = self.mMotor1Axis.getPosition()
                    if not is_singleaxis:
                        m2pos_imend = self.mMotor2Axis.getPosition()
                    

            else :
                # Wait for finishing
                cameraReady = CC.check_exposure_finished()
                if not cameraReady:
                    time.sleep(1)
                else:
                    m1pos_imend = self.mMotor1Axis.getPosition()
                    if not is_singleaxis:
                        m2pos_imend = self.mMotor2Axis.getPosition()
    

        # Here we need to stop movements and cancel camera recordings
        CC.stop_camera()
        self.mMotor1Axis.stopMovement()
        if not is_singleaxis :
            self.mMotor2Axis.stopMovement()

        if not self.mScanRunning:
            finishMessage = 'user cancelled the scan'
            
        # Finally close the log file
        logfile.close()
        
        print 'Scan thread ended : ' + finishMessage

        
# If the program is run directly or passed as an argument to the python
# interpreter then create a HelloWorld instance and show it
if __name__ == "__main__":
    dc = DiffControl()
    dc.run()
