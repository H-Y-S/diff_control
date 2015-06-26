from motor_axis import MotorAxis
import pygtk
pygtk.require('2.0')
import gtk
import threading
from time import sleep
from time import clock
import can_control


class RotationApp:
    MODE_WAITING = 1
    MODE_STARTING = 2
    MODE_STARTED = 3
    MODE_STOPPING = 4
    MODE_ZEROING = 5

    def __init__(self):
        # initialize the axes
        self.mAxRot = MotorAxis(3)
        self.mMode = RotationApp.MODE_WAITING
        self.mIsRunning = True

        self.builder = gtk.Builder()
        self.builder.add_from_file("continuous_rotation_app.glade")
        self.builder.connect_signals(self)        
        self.read_values_from_canbus()
        self.builder.get_object("startStopButton").set_label("Start")            

        gtk.gdk.threads_init()
        
        self.mControlThread = threading.Thread(target = self.controlMethod)
        self.mControlThread.start()

            
    # This one is called when the main window is destroyed (i.e. when  
    # delete_event returns null)
    def on_main_window_destroy(self, widget, data=None):
        print "destroy signal occurred"
        self.mIsRunning = False
        self.mControlThread.join()

        gtk.main_quit()
        can_control.releasePCAN()

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
        return False

    def controlMethod(self):
        while self.mIsRunning:
            gtk.threads_enter()
            try:
                # read motor position 
                rotval = self.read_values_from_canbus() # updates the display too
                
                if self.mMode == RotationApp.MODE_STARTED: # movement going on
                    # stop if target reached 
                    if self.mAxRot.finishedMoving():
                        self.mAxRot.finishMovement()
                        # change the button labels
                        # change button labels
                        self.builder.get_object("startStopButton").set_label("Start")
                        self.builder.get_object("goToZeroButton").set_sensitive(True)
                        self.mMode = RotationApp.MODE_WAITING
                        print "Finished movement in " + str(clock()-self.mStartTime) + " s"

                elif self.mMode == RotationApp.MODE_STARTING:
                    # change button labels
                    self.builder.get_object("startStopButton").set_label("Stop")
                    self.builder.get_object("goToZeroButton").set_sensitive(False)
                    
                    # read the values from controls
                    ctime = self.builder.get_object("counting_time_spin").get_value()
                    cangle = self.builder.get_object("rot_angle_spin").get_value()
                    degpersecond = cangle / ctime
                    print "Going to angle " + str(cangle) + " with speed " + str(degpersecond) + " deg/s"

                    # start the movement
                    self.mAxRot.startMoving(newPos = int(cangle),newSpeed = degpersecond)
                    self.mStartTime = clock()
                    self.mMode = RotationApp.MODE_STARTED
                    
                    
                elif self.mMode == RotationApp.MODE_STOPPING:
                    # stop motor movements
                    self.mAxRot.stopMovement()
                    
                    # change button labels
                    self.builder.get_object("startStopButton").set_label("Start")
                    self.builder.get_object("goToZeroButton").set_sensitive(True)

                    self.mMode = RotationApp.MODE_WAITING
                    
                elif self.mMode == RotationApp.MODE_ZEROING:
                    if self.mAxRot.finishedMoving():
                        print "Zeroing, movement finished"
                        self.mAxRot.finishMovement()
                        self.builder.get_object("startStopButton").set_sensitive(True)
                        self.mMode = RotationApp.MODE_WAITING

                                               
            finally:
                gtk.threads_leave()
                
            sleep(0.1)
                
        print "thread finishing"

    def read_values_from_canbus(self):
        val = self.mAxRot.getPosition()
        self.builder.get_object("position_display").set_text(str(val))
        return val
        

    def onGoToZeroClicked(self, widget, data = None):
#        self.builder.get_object("startStopButton").set_sensitive(False)
        self.mMode = RotationApp.MODE_ZEROING
        self.mAxRot.startMoving(0)
        
    
        
    def onStartStopClicked(self, widget, data = None):
        if self.mMode == RotationApp.MODE_WAITING:
            self.mMode = RotationApp.MODE_STARTING
        elif self.mMode == RotationApp.MODE_STARTED:
            self.mMode = RotationApp.MODE_STOPPING
            
    def onDebugClicked(self, widget, data = None):
        print "Debug information:"
        print "Status word : " + self.mAxRot.getStatus()
        print "Control word: " + self.mAxRot.getControlWord()
        print "Target posit: " + self.mAxRot.getTargetPosition()
        print "Current posi: " + self.mAxRot.getPositionHex()
        print "Pos Demand  : " + self.mAxRot.getPositionDemandValue()


    def onResetErrorsClicked(self, widget, data = None):
        self.mAxRot.resetErrors()
       
    def run(self):
        self.builder.get_object("main_window").show_all()
        gtk.threads_enter()
        gtk.main()
        gtk.threads_leave()

def main():
    ra = RotationApp()
    ra.run()


if __name__ == '__main__':
    main()

