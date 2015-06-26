from PCANBasic import *
import sys
import time

objPCANBasic = None
pcanStatusMessage = ""
pcanHandle = PCAN_PCIBUS1;


def initializePCAN():
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle

    try:
        if objPCANBasic == None:
            objPCANBasic = PCANBasic()
            
            result = objPCANBasic.GetValue(pcanHandle,PCAN_CHANNEL_CONDITION)
            if  (result[0] == PCAN_ERROR_OK) and (result[1] == PCAN_CHANNEL_AVAILABLE):
                result = objPCANBasic.Initialize(pcanHandle,PCAN_BAUD_1M)
                if result == PCAN_ERROR_OK:
                    pcanStatusMessage = "PCAN Initialized"
                else:
                    pcanStatusMessage = "PCAN Initialization failed"
                    objPCANBasic = None
            else:
                objPCANBasic = None
                print("PCAN Occupied")
                print result
                pcanStatusMessage = "PCAN Occupied"
    except:
        print "Exception on PCANBasic.Initialize: ", sys.exc_info()[0]
        objPCANBasic = None
        pcanStatusMessage = "PCAN Exception Occurred during Initialization"


def releasePCAN():
    if isInitialized():
        try:
            objPCANBasic.Uninitialize(pcanHandle)
        except:
            print "Could not release the PCAN"
    else:
        print "PCAN not initilized: will not uninitialize"

def isInitialized():
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle

    return not objPCANBasic == None



# This will set DOUT1 of Axis 0 (Y-axis) to be 1
# affects all motors except rotation
def enableMotors():
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle
    nodeAddress = 1
    cobid = "60" + str(nodeAddress)

    try:
        if not objPCANBasic == None:
            msgs,tmp = readAllCANMessages() # clear the buffer, should be empty
            if len(msgs) > 0:
                print "Response buffer has unprocessed messages: "
                print msgs

            sendCANMsg(cobid,["23","FE","60","01","00","00","01","00"]);    
            canMSGs,canMSGsraw = waitCANResponse(nodeAddress,"FE","60")

            cmd,index,subindex,data = getCANMessageParts(canMSGsraw[0])
            return data & 0xffff           
        else:
            print "Can not available"
            return 0x0000
    except:
        print "exception:",sys.exc_info()[0]
        raise
    

# This will set DOUT1 of Axis 0 (Y-axis) to be 0
# affects all motors except rotation
def disableMotors():
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle
    nodeAddress = 1
    cobid = "60" + str(nodeAddress)

    try:
        if not objPCANBasic == None:
            msgs,tmp = readAllCANMessages() # clear the buffer, should be empty
            if len(msgs) > 0:
                print "Response buffer has unprocessed messages: "
                print msgs

            sendCANMsg(cobid,["23","FE","60","01","00","00","00","00"]);    
            canMSGs,canMSGsraw = waitCANResponse(nodeAddress,"FE","60")

            cmd,index,subindex,data = getCANMessageParts(canMSGsraw[0])
            return data & 0xffff           
        else:
            print "Can not available"
            return 0x0000
    except:
        print "exception:",sys.exc_info()[0]
        raise



# Sends a message to the CAN bus
# cobid is the COB-ID as a string
# msg is the message in the format ["00","bb","ef",...]
# returns True if send succeeds False if not
def sendCANMsg(cobid,msg):
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle

    try:
        if not objPCANBasic == None:
            canmsg = TPCANMsg()
            canmsg.ID = int(cobid,16)
            canmsg.LEN = 8
            canmsg.MSGTYPE = PCAN_MESSAGE_STANDARD
            for i in range(len(msg)):
                canmsg.DATA[i] = int(msg[i],16);
            
            return objPCANBasic.Write(pcanHandle, canmsg) == PCAN_ERROR_OK
        else:
            return False
    except:
        print "Exception on PCANBasic.Write: ", sys.exc_info()[0]


# nodeAddress is the address of the node to access (1,2,3,...)
# returns the status word as an integer (32 bit) in native endianness
def getCANStatusWord(nodeAddress):
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle
    cobid = "60" + str(nodeAddress)

    try:
        if not objPCANBasic == None:
            msgs,tmp = readAllCANMessages() # clear the buffer, should be empty
            if len(msgs) > 0:
                print "Response buffer has unprocessed messages: "
                print msgs

            sendCANMsg(cobid,["40","41","60","00","00","00","00","00"]);    
            canMSGs,canMSGsraw = waitCANResponse(nodeAddress,"41","60")

            cmd,index,subindex,data = getCANMessageParts(canMSGsraw[0])
            return data & 0xffff           
        else:
            print "Can not available"
            return 0x0000
    except:
        print "exception:",sys.exc_info()[0]
        raise



# nodeAddress is the address of the node to access (1,2,3,...)
# returns the control word as an integer (32 bit) in native endianness
def getCANControlWord(nodeAddress):
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle
    cobid = "60" + str(nodeAddress)

    try:
        if not objPCANBasic == None:
            msgs,tmp = readAllCANMessages() # clear the buffer, should be empty
            if len(msgs) > 0:
                print "Response buffer has unprocessed messages: "
                print msgs

            sendCANMsg(cobid,["40","40","60","00","00","00","00","00"]);    
            canMSGs,canMSGsraw = waitCANResponse(nodeAddress,"40","60")
#            canMSGs,canMSGsraw = readAllCANMessages()
            cmd,index,subindex,data = getCANMessageParts(canMSGsraw[0])
            return data & 0xffff           
        else:
            print "Can not available"
            return 0x0000
    except:
        print "exception:",sys.exc_info()[0]
        raise



# nodeAddress is the address of the node to access (1,2,3,...)
# returns the target position as an integer (32 bit) in native endianness
def getCANTargetPosition(nodeAddress):
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle
    cobid = "60" + str(nodeAddress)

    try:
        if not objPCANBasic == None:
            msgs,tmp = readAllCANMessages() # clear the buffer, should be empty
            if len(msgs) > 0:
                print "Response buffer has unprocessed messages: "
                print msgs

            sendCANMsg(cobid,["40","7a","60","00","00","00","00","00"]);    
            canMSGs,canMSGsraw = waitCANResponse(nodeAddress,"7a","60")
#            canMSGs,canMSGsraw = readAllCANMessages()
            cmd,index,subindex,data = getCANMessageParts(canMSGsraw[0])
            return data           
        else:
            print "Can not available"
            return 0x00000000
    except:
        print "exception:",sys.exc_info()[0]
        raise

# nodeAddress is the address of the node to access (1,2,3,...)
# returns the target position as an integer (32 bit) in native endianness
def getCANPositionDemandValue(nodeAddress):
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle
    cobid = "60" + str(nodeAddress)

    try:
        if not objPCANBasic == None:
            msgs,tmp = readAllCANMessages() # clear the buffer, should be empty
            if len(msgs) > 0:
                print "Response buffer has unprocessed messages: "
                print msgs

            sendCANMsg(cobid,["40","fc","60","00","00","00","00","00"]);    
            canMSGs,canMSGsraw = waitCANResponse(nodeAddress,"fc","60")
#            canMSGs,canMSGsraw = readAllCANMessages()
            cmd,index,subindex,data = getCANMessageParts(canMSGsraw[0])
            return data           
        else:
            print "Can not available"
            return 0x00000000
    except:
        print "exception:",sys.exc_info()[0]
        raise


# nodeAddress is the address of the node to access (1,2,3,...)
# returns the motor position as an integer (32 bit) in native endianness
def getMotorPos(nodeAddress):
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle
    cobid = "60" + str(nodeAddress)

    try:
        if not objPCANBasic == None:
            msgs,tmp = readAllCANMessages() # clear the buffer, should be empty
            if len(msgs) > 0:
                print "Response buffer has unprocessed messages: "
                print msgs

            command = ["40","63","60","00","00","00","00","00"]
            sendCANMsg(cobid,command);    
            canMSGs,canMSGsraw = waitCANResponse(nodeAddress,"63","60")
#            canMSGs,canMSGsraw = readAllCANMessages()
            cmd,index,subindex,data = getCANMessageParts(canMSGsraw[0])
            return data           
        else:
            print "Can not available"
            return 0x00000000
    except:
        print "exception:",sys.exc_info()[0]
        raise


def prepareMovement(nodeAddress):
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle
    cobid = "60" + str(nodeAddress)


    # TODO: Add enable signal to allow motor movements
    if not objPCANBasic == None:        
        msgs,tmp = readAllCANMessages() # clear the buffer, should be empty
        if len(msgs) > 0:
            print "Response buffer has unprocessed messages: "
            print msgs

        # Disable the joystick
        sendCANMsg(cobid,["23","18","21","00","FF","00","00","00"])
        canMSGs,canMSGsraw = waitCANResponse(nodeAddress,"18","21")



def resetErrors(nodeAddress):
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle
    cobid = "60" + str(nodeAddress)
    if not objPCANBasic == None:        
        msgs,tmp = readAllCANMessages() # clear the buffer, should be empty
        if len(msgs) > 0:
            print "Response buffer has unprocessed messages: "
            print msgs

        # Reset fault
        sendCANMsg(cobid,["23","40","60","00","86","00","00","00"])
        canMSGs,canMSGsraw = waitCANResponse(nodeAddress,"40","60")
#        time.sleep(0.1)
        print "6040:86 :: {0:08X}".format(getCANStatusWord(nodeAddress))

        sendCANMsg(cobid,["23","40","60","00","06","00","00","00"])
        canMSGs,canMSGsraw = waitCANResponse(nodeAddress,"40","60")
#        time.sleep(0.1)
        print "6040:06 :: {0:08X}".format(getCANStatusWord(nodeAddress))

        sendCANMsg(cobid,["23","40","60","00","07","00","00","00"])
        canMSGs,canMSGsraw = waitCANResponse(nodeAddress,"40","60")
#        time.sleep(0.1)
        print "6040:07 :: {0:08X}".format(getCANStatusWord(nodeAddress))

        sendCANMsg(cobid,["23","40","60","00","3F","00","00","00"])
        canMSGs,canMSGsraw = waitCANResponse(nodeAddress,"40","60")
#        time.sleep(0.1)
        print "6040:3F :: {0:08X}".format(getCANStatusWord(nodeAddress))
        


def finishMovement(nodeAddress):
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle
    cobid = "60" + str(nodeAddress)

    # TODO: add check to make sure we have stopped
    # TODO: make sure we are in a good state after stopping

    if not objPCANBasic == None:        
        msgs,tmp = readAllCANMessages() # clear the buffer, should be empty
        if len(msgs) > 0:
            print "Response buffer has unprocessed messages: "
            print msgs

        # Enable the joystick again
        sendCANMsg(cobid,["23","18","21","00","FE","00","00","00"])
        canMSGs,canMSGsraw = waitCANResponse(nodeAddress,"18","21")

# nodeAddress is the address of the node to access (1,2,3,...)
# newpos is the new position value as an integer in native endianness
# return True on success, False on failure
def setMotorPos(nodeAddress,newpos,speed):
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle
    cobid = "60" + str(nodeAddress)

    try:            
        if not objPCANBasic == None:        
            msgs,tmp = readAllCANMessages() # clear the buffer, should be empty
            if len(msgs) > 0:
                print "Response buffer has unprocessed messages: "
                print msgs

            # Set the new target position (by default control word is 3F, 
            # so the move starts immediately
            command = ["23","7a","60","00"] + convertIntegerToByteArray(newpos)
            print "Moving to pos: " + str(newpos)
            print "Sending command: " + str(command)
            print "{0:08X}".format(getCANStatusWord(nodeAddress))
            sendCANMsg(cobid,command);
            canMSGs,canMSGsraw = waitCANResponse(nodeAddress,"7a","60")

            # Set the new targeting speed
            command = ["23","81","60","00"] + convertIntegerToByteArray(speed)
            print "Sending command: " + str(command)            
            sendCANMsg(cobid,command);
            canMSGs,canMSGsraw = waitCANResponse(nodeAddress,"81","60")

            # Busy wait for movement finish
            sw = getCANStatusWord(nodeAddress)
            while (sw & 0x00000400) == 0x00000000:
                time.sleep(0.1)
                sw = getCANStatusWord(nodeAddress)
                            
            canMSGs,canMSGsraw = readAllCANMessages() # This probably returns empty!!
            printCANMSGs(canMSGsraw)
            return True
        else:
            print "CAN not available"
            return False
    except:
        print "exception:",sys.exc_info()[0]
        raise


# nodeAddress is the address of the node to access (1,2,3,...)
# newpos is the new position value as an integer in native endianness
# return True on success, False on failure
def startMotorMovementToPos(nodeAddress,newpos,speed):
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle
    cobid = "60" + str(nodeAddress)

    try:            
        if not objPCANBasic == None:        
            msgs,tmp = readAllCANMessages() # clear the buffer, should be empty
            if len(msgs) > 0:
                print "Response buffer has unprocessed messages: "
                print msgs

            # Set the new target position (by default control word is 3F, 
            # so the move starts immediately
            command = ["23","7a","60","00"] + convertIntegerToByteArray(newpos)
            print "Moving to pos: " + str(newpos)
            print "Sending command: " + str(command)
#            print "{0:08X}".format(getCANStatusWord(nodeAddress))
            sendCANMsg(cobid,command);
            canMSGs,canMSGsraw = waitCANResponse(nodeAddress,"7a","60",True)

            # Set the new targeting speed
            command = ["23","81","60","00"] + convertIntegerToByteArray(speed)
            print "Sending command: " + str(command)            
            sendCANMsg(cobid,command);
            canMSGs,canMSGsraw = waitCANResponse(nodeAddress,"81","60",True)

#            canMSGs,canMSGsraw = readAllCANMessages()
#            print "Response messages in the buffer:"
#            printCANMSGs(canMSGsraw)
            print "New target pos is: " + str(getCANTargetPosition(nodeAddress))
            return True
        else:
            print "CAN not available"
            return False
    except:
        print "exception:",sys.exc_info()[0]
        raise

# nodeAddress is the address of the node to access (1,2,3,...)
# newpos is the new position value as an integer in native endianness
# return True on success, False on failure
def stopMotorMovement(nodeAddress):
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle
    cobid = "60" + str(nodeAddress)

    try:            
        if not objPCANBasic == None:        
            msgs,tmp = readAllCANMessages() # clear the buffer, should be empty
            if len(msgs) > 0:
                print "Response buffer has unprocessed messages: "
                print msgs

            # Set the control word to stop movement
            command = ["23","40","60","00","3B","00","00","00"] 
            print "stopping axis: " + str(nodeAddress)
            print "{0:08X}".format(getCANStatusWord(nodeAddress))
            sendCANMsg(cobid,command);
            finishMovement(nodeAddress)
            canMSGs,canMSGsraw = waitCANResponse(nodeAddress,"40","60")

            return True
        else:
            print "CAN not available"
            return False
    except:
        print "exception:",sys.exc_info()[0]
        raise



def isMotorMovementFinished(nodeAddress):
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle
    cobid = "60" + str(nodeAddress)

    try:            
        if not objPCANBasic == None:        
            msgs,tmp = readAllCANMessages() # clear the buffer, should be empty
            if len(msgs) > 0:
                print "Response buffer has unprocessed messages: "
                print msgs

            sw = getCANStatusWord(nodeAddress)
 #           canMSGs,canMSGsraw = waitCANResponse(cobid,"40","60")

#            canMSGs,canMSGsraw = readAllCANMessages()
            return (sw & 0x00000400) != 0x00000000
        else:
            print "CAN not available"
            return False
    except:
        print "exception:",sys.exc_info()[0]
        raise


    

# returns message parts
def getCANMessageParts(msg):
    cmd = msg.DATA[0]
    index = msg.DATA[2] << 8 | msg.DATA[1]
    subindex = msg.DATA[3]

    if msg.LEN == 6:
        data = msg.DATA[5]<<8 | msg.DATA[4]
    elif msg.LEN == 8:
        data = msg.DATA[7]<<24 | msg.DATA[6]<<16 | msg.DATA[5]<<8 | msg.DATA[4]
        # Convert the data to signed value
        if (data > 0x7fffffff):
            print "Converting to signed"
            data = data - 0x100000000        

    return cmd,index,subindex,data
    

# Converts a 32 bit integer to little endian
# array of bytes as strings    
def convertIntegerToByteArray(val):
    s = tohex(val,32)[2:].zfill(8)  # remove the 0x from the start and
                                    # fill to 8 bytes width
    res = []
    res.append(s[6:8]) # low byte first
    res.append(s[4:6])
    res.append(s[2:4])
    res.append(s[0:2])
    return res



def tohex(val, nbits):
    return hex(int((val + (1 << nbits)) % (1 << nbits)))

    
# Takes a CAN bus message and formats it as a string for display purposes
def formatCANMessageAsString(msg):
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle

    strTmp = "{0:03X}".format(msg.ID) + " "
    for i in range(msg.LEN):
        strTmp = strTmp + " " + "{0:02X}".format(msg.DATA[i])

    return strTmp


## Reads all messages from the CAN bus and returns the messages as an
## array of strings
def readAllCANMessages():
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle

    try:
        if not objPCANBasic == None:
#            time.sleep(0.1);
            result = objPCANBasic.Read(pcanHandle)
            
            i = 0
            msg = result[1]
            canMSGs = []
            canMSGsraw = []
            
            while msg.LEN > 0:
                canMSGs.append(formatCANMessageAsString(msg))
                canMSGsraw.append(msg)
                i = i + 1
                result = objPCANBasic.Read(pcanHandle)
                msg = result[1];

#            printCANMSGs(canMSGsraw)
            
            return canMSGs,canMSGsraw
        else:
            return None
    except:
        print "exception:",sys.exc_info()[0]
        raise
        return None
        

# waits for a CAN response and checks if that matches
# the cobid and cmdbyte1 and cmdbyte2
# ex. cobid = 503, cmdbyte2 = "40", cmdbyte1 = "60"
# returns the result from readAllCANMessages
def waitCANResponse(nodeAddress,cmdbyte2,cmdbyte1,printout=False):
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle

    responseid = 0x580 + nodeAddress

    try:
        if not objPCANBasic == None:
            canMSGs=[]
            while len(canMSGs) == 0:
                canMSGs,canMSGsraw = readAllCANMessages()
                time.sleep(0.01)

            if len(canMSGs) > 0:
                msg = canMSGsraw[0]
                commandSuccess = msg.DATA[0] != int("80",16)

                isOK = (msg.ID == responseid)
                isOK = isOK & (msg.DATA[1] == int(cmdbyte2,16))
                isOK = isOK & (msg.DATA[2] == int(cmdbyte1,16))

                if isOK != True:
                    print "Message is not a matching response"
                    print "Read message: " + canMSGs[0]
                    print "COBID: " + str(responseid) + " byte2: " + cmdbyte2 + " byte1: " + cmdbyte1
                    print "COBID data: " + str(msg.ID) + " byte2 data: " + str(msg.DATA[1]) + " byte1 data: " + str(msg.DATA[2])
            if isOK & printout:
                print "Matching response: " + canMSGs[0]

            if isOK & (not commandSuccess):
                print "Matching respons with error code"
                printCANMSGs(canMSGsraw)

            return canMSGs,canMSGsraw
        else:
            return None
    except:
        print "exception:",sys.exc_info()[0]
        raise
        return None

        



def printCANMSGs(msgs):
    for i in range(0,len(msgs)):
        print formatCANMessageAsString(msgs[i])
        
def main():
    global objPCANBasic
    global pcanStatusMessage
    global pcanHandle
    nodeAddress = 3
    
    initializePCAN()
    print pcanStatusMessage
    print "{0:04X}".format(getCANStatusWord(nodeAddress))
    releasePCAN()
    
    
    
    


if __name__ == '__main__':
    main()


