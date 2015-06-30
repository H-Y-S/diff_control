# This class can be used to set and get parameter values from the camera
# as well as to initiate/interrupt image taking
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
        self.mTN.write("exptime")

        # Read response
        resp = self.mTN.read_some()

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
        self.mTN.write("exptime " + str(newExpTime))

        # Read response
        resp = self.mTN.read_some()

        # Parse response
        code,resp = resp.split(' ',1)
        ok,resp = resp.split(' ',1)
 
        # Return true on success false on error
        if (not ok == "OK"):		
            return False
        else:
            return True
            


    def get_img_path(self):
         # Send command
        self.mTN.write("imgpath")

        # Read response
        resp = self.mTN.read_some()
        code,resp = resp.split(' ',1)
        ok,resp = resp.split(' ',1)
        
        return resp[:-1]


    # Sets the path for saved images
    # the given path is in given in the 
    # path space available on the 
    # camera control computer
    def set_img_path(self,newpath):
        # Send command
        self.mTN.write("imgpath " + newpath)
        
        # Read response
        resp = self.mTN.read_some()
        code,resp = resp.split(' ',1)
        ok,resp = resp.split(' ',1)

        if (ok == "OK"):
            return resp[:-1], True
        else:
            return resp, False


    def close_connection(self):
        self.mTN.read_very_eager()
        self.mTN.close()



class CamTest:
    def __init__(self):
        self.builder = gtk.Builder()
        self.builder.add_from_file("camera_test.glade")

        self.builder.connect_signals(self)
        
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
        cc = CameraComm("10.110.11.141",41234)
        val = cc.get_exp_time()
        self.builder.get_object('exp_time_entry').set_text(val)
        val = cc.get_img_path()
        print val
        self.builder.get_object('img_path_entry').set_text(val)
        cc.close_connection()    

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
