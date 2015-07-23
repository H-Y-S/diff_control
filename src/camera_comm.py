# This class can be used to set and get parameter values from the camera
# as well as to initiate/interrupt image taking
# The Pilatus/Dectris camserver seems to end all strings sent with characted \030 (==\x18)
# we can check for this character to be sure to read until end of each reply
import telnetlib
import pygtk
pygtk.require('2.0')
import gtk

class CameraComm:
    # Constructor
    def __init__(self,host,port):
        # Establish a telnet connection
        self.mTN = telnetlib.Telnet(host,port)


    # checks if camserver responds and that we are in
    # operative mode (not read only mode)
    def check_connection(self):
        isOK = True
        message = "Connection to Pilatus established"
        
        # Check connection
        et = get_exp_time()
        if et < 0:
            # problem with the connection
            isOK = False
            message = "Cannot connect to Pilatus\nCheck that the control computer is on, camserver running, and TVX not running"
        else: 
            # Check to modify
            setok = set_exp_time(et)
            if not setok:
                isOK = False
                message = "Cannot control Pilatus\nCheck that the TVX is not running"
               
        
        return isOK, message


    def get_exp_time(self):
        # Send command
        self.mTN.write("exptime\r\n")

        # Read response
        resp = self.read_response() 
        print "Resp is:::" + resp + ":::"

        # Parse response
        time = -1
        code,resp = resp.split(' ',1)
        ok,resp = resp.split(' ',1)
        if (ok == "OK"):		
            tmp,resp = resp.split(':',1)
            time,resp = resp.strip().split(' ',1)

        # Return the answer (-1 on error)
        return time

    def set_exp_time(self,newExpTime):
        # Send command
        self.mTN.write("exptime " + str(newExpTime) + "\r\n")

        # Read response
        resp = self.read_response() 
        print "Resp is:::" + resp + ":::"

        # Parse response
        code,resp = resp.split(' ',1)
        ok,resp = resp.split(' ',1)
 
        # Return true on success false on error
        if (not ok == "OK"):		
            return False
        else:
            return True

            
    def get_exp_period(self):
        # Send command
        self.mTN.write("expperiod\r\n")

        # Read response
        resp = self.read_response() 
        print "Resp is:::" + resp + ":::"

        # Parse response
        time = -1
        code,resp = resp.split(' ',1)
        ok,resp = resp.split(' ',1)
        if (ok == "OK"):		
            tmp,resp = resp.split(':',1)
            time,resp = resp.strip().split(' ',1)

        # Return the answer (-1 on error)
        return time


    def set_exp_period(self,newExpPeriod):
        # Send command
        self.mTN.write("expperiod " + str(newExpPeriod) + "\r\n")

        # Read response
        resp = self.read_response() 
        print "Resp is:::" + resp + ":::"

        # Parse response
        code,resp = resp.split(' ',1)
        ok,resp = resp.split(' ',1)
 
        # Return true on success false on error
        if (not ok == "OK"):		
            return False
        else:
            return True


    # Start the camera exposure with the given imageFileName
    # Communication protocol looks like this:
    # exposure kuva0008.tif
    # 15 OK  Starting 5.000000 second background: 2015-Jun-30T22:21:38.545
    # 7 OK /data/01/HS/pilatus_testing/kuva0008.tif        
    def expose_image(self,imageFileName):
        # Send command
        camera_command = "exposure " + imageFileName
        print "exposing with command: " + camera_command
        self.mTN.write(camera_command + "\r\n")

        # The first response is to acknowledge the acquisition start
        # Read response
        resp = self.read_response() 
        print "Resp is:::" + resp + ":::"

        # Parse response
        code,resp = resp.split(' ',1)
        ok,resp = resp.split(' ',1)

        # Very short exposure might have finished already
        # Actually with the new way of reading until 0x18, this
        # ready response is not read at this point
        expfinished = False
        try :
            resp.index('7 OK')
            expfinished = True
        except ValueError :
            expfinished = False

        
        # After image acquisition finishes, there will be another acknowledgement
        # Return true on success false on error
        if (not ok == "OK"):		
            return False,expfinished
        else:
            return True,expfinished

    # This is a non-blocking way to check if image has been read already 
    def check_exposure_finished(self) :
        # Read whatever is available
        resp = self.mTN.read_eager()
        print "Resp is:::" + resp + ":::"

        # If resp is not empty, and does not end with \030, read until \030 encountered
        if not (resp == '') and not (resp[-1] == chr(0x18)):
            resp = resp + self.read_response() # read until 0x18
            
        try :
            resp.index('7 OK')
            return True
        except ValueError :
            return False
                
        
    # This stops the camera reading
    # Example of using stop
    # exposure kuva0010.tif
    # 15 OK  Starting 5.000000 second background: 2015-Jun-30T22:22:16.376
    # ResetCam
    # 7 ERR /data/01/HS/pilatus_testing/kuva0010.tif15 OK
    def stop_camera(self):
        # ResetCam seems the more appropriate command for stopping the camera
        # Send command
        self.mTN.write("ResetCam\r\n")
        
        


    def get_img_path(self):
         # Send command
        self.mTN.write("imgpath\r\n")

        # Read response
        resp = self.read_response()
        print "Resp is:::" + resp + ":::"

        code,resp = resp.split(' ',1)
        ok,resp = resp.split(' ',1)
        
        return resp[:-1]


    # Sets the path for saved images
    # the given path is given in the 
    # path space available on the 
    # camera control computer
    def set_img_path(self,newpath):
        # Send command
        print "setting image save path as: " + newpath
        self.mTN.write("imgpath " + newpath + "\r\n")
        
        # Read response
        resp = self.read_response()
        print "Resp is:::" + resp + ":::"

        code,resp = resp.split(' ',1)
        ok,resp = resp.split(' ',1)

        if (ok == "OK"):
            return resp[:-1], True
        else:
            return resp, False


    def close_connection(self):
        resp = self.mTN.read_very_eager()
        print "Resp is:::" + resp + ":::"

        self.mTN.close()

    def read_response(self):
        resp = self.mTN.read_until(chr(0x18))
        return resp


class CamTest:
    def __init__(self):
        self.builder = gtk.Builder()
        self.builder.add_from_file("camera_test.glade")

        self.builder.connect_signals(self)
        self.mCC = CameraComm("10.110.11.141",41234)
        
        self.read_values_from_server()
        
    # This one is called when the main window is destroyed (i.e. when  
    # delete_event returns null)
    def on_main_window_destroy(self, widget, data=None):
        print "destroy signal occurred"
        gtk.main_quit()

    # This one is called when the main window close-button is clicked
    def on_main_window_delete_event(self, widget, event, data=None):
        # If you return FALSE in the "delete_event" signal handler,
        # GTK will emit the "destroy" signal. Returning TRUE means
        # you don't want the window to be destroyed.
        # This is useful for popping up 'are you sure you want to quit?'
        # type dialogs.
        print "delete event occurred"
        self.mCC.close_connection()    

        # Change FALSE to TRUE and the main window will not be destroyed
        # with a "delete_event".
        # TODO: Confirm close
        return False

    def update_settings(self, widget, data=None):
        print("Updating the settings")
        self.write_values_to_server()
        self.read_values_from_server()
        
    def take_image(self, widget, data=None):
        print("Taking an image")
    
    
    def read_values_from_server(self):
        val = self.mCC.get_exp_time()
        self.builder.get_object('exp_time_entry').set_text(val)
        val = self.mCC.get_img_path()
        print val
        self.builder.get_object('img_path_entry').set_text(val)


    def write_values_to_server(self):
        self.builder.get_object('exp_time_entry').get_text()
    
    def run(self):
        self.builder.get_object("main_window").show_all()
        gtk.main()

    
        
def main():
    ct = CamTest()
    ct.run()


if __name__ == '__main__':
    main()
