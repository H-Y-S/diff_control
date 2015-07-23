import can_control
import pygtk
pygtk.require('2.0')
import gtk


# These params come from cnc.ini file from the phoenix x-ray datos acq
MOTOR_AXIS_PARAMETERS = [
    # Axis 1 (Y) is not used now
    [2400,11976,0,1200000], 
    # Axis 2 (Z): steps/mm, steps at ref pos, ref pos (mm), pos speed (steps / 64 s)
    [1600,-9056,300,1200000], 
    # Axis 3 (R): steps/deg, steps at ref pos, ref pos (deg), pos speed (steps / 64 s)
    [1000,230360,0,1200000]
    ] 



class MotorAxis:
    
    def __init__(self, axisID):
        self.mAxisID = axisID
#        self.mCOBIDTx = "60"+str(self.mAxisID)
#        self.mCOBIDRx = "58"+str(self.mAxisID)
        
        # Get the ref values
        self.mStepsPerUnit = MOTOR_AXIS_PARAMETERS[axisID-1][0]
        self.mRefPosSteps = MOTOR_AXIS_PARAMETERS[axisID-1][1]
        self.mRefPos = MOTOR_AXIS_PARAMETERS[axisID-1][2]

        # Do not move at MAX speed
        self.mPosSpeed = MOTOR_AXIS_PARAMETERS[axisID-1][3] / 2
        
        # Initialize the PCAN if necessary
        if not can_control.isInitialized():
            can_control.initializePCAN()
        


    # Moves the motor to newPos given in user units
    # The move starts immediately and the call returns
    # once the move has ended
    def moveImmediateSynchronous(self, newPos, newSpeed=None):
        possteps = self.convertPosToSteps(newPos)
        
        if newSpeed == None:
            newSpeed = self.mPosSpeed
        else:
            # convert from degs / second to incrs / 64 seconds
            newSpeed = int(newSpeed * self.mStepsPerUnit * 64)
            print newSpeed
        
        if can_control.isInitialized():
            can_control.prepareMovement(self.mAxisID)
            can_control.resetErrors(self.mAxisID)
            can_control.setMotorPos(self.mAxisID,possteps,newSpeed)
            can_control.finishMovement(self.mAxisID)
        else:
            print "Could not move motors, CANBus is not intialized"
            



    # Starts a motor movement to a given position with a given speed
    # this call will return immediately after the motor has been instructed to move
    # (but the actual movement might start only after this call returns)
    #
    # when the movement is finished, finishMovement should be called
    def startMoving(self, newPos, newSpeed=None):
        possteps = self.convertPosToSteps(newPos)
        
        if newSpeed == None:
            newSpeed = self.mPosSpeed
        else:
            # convert from degs / second to incrs / 64 seconds
            newSpeed = int(newSpeed * self.mStepsPerUnit * 64)
            print newSpeed
        
        if can_control.isInitialized():
            can_control.prepareMovement(self.mAxisID)
            can_control.startMotorMovementToPos(self.mAxisID,possteps,newSpeed)
        else:
            print "Could not move motors, CANBus is not intialized"
            

    def finishMovement(self):
        can_control.finishMovement(self.mAxisID)


    def stopMovement(self):
        can_control.stopMotorMovement(self.mAxisID)
        

    def getPosition(self):
        if can_control.isInitialized():
            possteps = can_control.getMotorPos(self.mAxisID)
            
            # convert steps to pos in user units
            posval = self.convertStepsToPos(possteps)
            
            return posval
        else:
            print "Could not get motor position, CANBus is not intialized"
            return 0

    def getPositionHex(self):
        if can_control.isInitialized():
            return "{0:08X}".format(can_control.getMotorPos(self.mAxisID))
        else:
            print "Could not get motor position, CANBus is not intialized"
            return 0



    def resetErrors(self):
        can_control.resetErrors(self.mAxisID)
            
    def getStatus(self):
        return "{0:08X}".format(can_control.getCANStatusWord(self.mAxisID))
    
    def getControlWord(self):
        return "{0:08X}".format(can_control.getCANControlWord(self.mAxisID))

    def getTargetPosition(self):
        return "{0:08X}".format(can_control.getCANTargetPosition(self.mAxisID))

    # demand value is the one that is changed as the motor approaches the target value?
    def getPositionDemandValue(self):
        return "{0:08X}".format(can_control.getCANPositionDemandValue(self.mAxisID))


    # Returns boolean (true if moving has finished, false if
    # still moving)
    def finishedMoving(self):
        return can_control.isMotorMovementFinished(self.mAxisID)
            

    # Converts a position in real units to motor steps as hex string
    def convertPosToSteps(self,pos):
        posreal = int((pos-self.mRefPos) * self.mStepsPerUnit + self.mRefPosSteps)
        print pos
        print posreal
        return posreal

    # converts pos in steps to user units
    def convertStepsToPos(self,possteps):
        posval = float(possteps-self.mRefPosSteps) / self.mStepsPerUnit + self.mRefPos
        return posval

    # Return true if axis is connected, false if not connected
    def testConnection(self):
        return False

class MotorTest:
    def __init__(self):
        # initialize the axes
        self.mAxRot = MotorAxis(3)
        self.mAxZ = MotorAxis(2)
        self.mAxY = MotorAxis(1)

        # enable motor movement
        can_control.enableMotors()

        self.builder = gtk.Builder()
        self.builder.add_from_file("can_test.glade")
        self.builder.connect_signals(self)        
        self.read_values_from_canbus()
            
    # This one is called when the main window is destroyed (i.e. when  
    # delete_event returns null)
    def on_main_window_destroy(self, widget, data=None):
        print "destroy signal occurred"
        gtk.main_quit()
        can_control.disableMotors()
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


    def read_values_from_canbus(self):
        print "read_values_from_canbus"
        val = self.mAxRot.getPosition()
        print "rot status: " + str(self.mAxRot.getStatus())
        self.builder.get_object("rot_pos").set_text(str(val))

        val = self.mAxZ.getPosition()
        print "Z status: " + str(self.mAxZ.getStatus())
        self.builder.get_object("z_pos").set_text(str(val))
        

    def onReadValuesClicked(self, widget, data = None):
        print "onReadValuesClicked"
        self.read_values_from_canbus()
        
    def onMoveRotClicked(self, widget, data = None):
        try:
            val = float(self.builder.get_object("rot_pos").get_text())
        except:
            val = 0
        print "moving motor " + str(self.mAxRot.mAxisID) + " to " + str(val)
        self.mAxRot.moveImmediateSynchronous(val)
  
    def onMoveZClicked(self, widget, data = None):
        try:
            val = float(self.builder.get_object("z_pos").get_text())
        except:
            print "setting default 0 for z"
            val = 0
            raise
        print "moving motor " + str(self.mAxZ.mAxisID) + " to " + str(val)
        self.mAxZ.moveImmediateSynchronous(val)
                  
    def run(self):
        self.builder.get_object("main_window").show_all()
        gtk.main()

def main():
    mt = MotorTest()
    mt.run()


if __name__ == '__main__':
    main()

