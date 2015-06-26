from PCANBasic import *
import time

class PCANTest:
    ## Constructor
    def __init__(self):
        self.objPCANBasic = PCANBasic()
        self.PcanHandle = PCAN_PCIBUS1;
        
        # Check that the device is available
        result = self.objPCANBasic.GetValue(self.PcanHandle,PCAN_CHANNEL_CONDITION);
        if  (result[0] == PCAN_ERROR_OK) and (result[1] == PCAN_CHANNEL_AVAILABLE):
            print("PCAN Available");
        else:
            print("PCAN Occupied");        

        print result
        print self.objPCANBasic.Initialize(self.PcanHandle,PCAN_BAUD_1M)

        # print the configuration
#        self.printPDOConf();

        # Write PDOs
        self.readAllCANMessages()
        self.sendCANMsg("603",["40","41","60","00","00","00","00","00"]); # read the status word message
#        self.sendCANMsg("603",["23","40","60","00","86","00","00","00"]); # switch off or ready to start
#        time.sleep(1);
#        self.sendCANMsg("603",["40","41","60","00","00","00","00","00"]); # read the status word message
#        self.sendCANMsg("603",["23","40","60","00","87","00","00","00"]); # switch off or ready to start
#        self.sendCANMsg("603",["40","41","60","00","00","00","00","00"]); # read the status word message
#        self.sendCANMsg("603",["23","40","60","00","0f","00","00","00"]); # switch on
#        self.sendCANMsg("603",["40","41","60","00","00","00","00","00"]); # read the status word message
#        self.sendCANMsg("603",["23","40","60","00","3f","00","00","00"]); # enable move
#        self.sendCANMsg("603",["40","41","60","00","00","00","00","00"]); # read the status word message
#        self.sendCANMsg("303",["80","4f","12","00","00","00","00","00"]); # write the speed (4 bytes)
#        time.sleep(1);
#        self.sendCANMsg("080",["00","00","00","00","00","00","00","00"]); # write the sync message
#        time.sleep(1);
#        self.sendCANMsg("203",["AA","AA","00","00","3F","00","00","00"]); # write the target position (4 bytes) and control word (2 bytes)
#        time.sleep(1);
#        self.sendCANMsg("080",["00","00","00","00","00","00","00","00"]); # write the sync message
#        self.sendCANMsg("203",["AA","AA","00","00","0F","00","00","00"]); # write the target position (4 bytes) and control word (2 bytes)
#        self.sendCANMsg("080",["00","00","00","00","00","00","00","00"]); # write the sync message
#        self.sendCANMsg("603",["40","41","60","00","00","00","00","00"]); # read the status word message
#        self.sendCANMsg("603",["40","7a","60","00","00","00","00","00"]); # read the target position
#        self.sendCANMsg("603",["40","81","60","00","00","00","00","00"]); # read the target speed

        self.sendCANMsg("603",["23","18","21","00","FF","00","00","00"]);
        self.sendCANMsg("603",["40","18","21","00","00","00","00","00"]);
        self.sendCANMsg("603",["23","81","60","00","00","AA","0A","00"]);
        self.sendCANMsg("603",["23","7a","60","00","00","00","00","00"]);
        self.sendCANMsg("603",["40","7a","60","00","00","00","00","00"]); # read the target position
        self.sendCANMsg("603",["40","81","60","00","00","00","00","00"]); # read the target speed
        self.readAllCANMessages()

        # First try to read the current axis positions
        # Write a command
        # COB ID: 0x603 == 0x600 + NODE ID (Rx SDO)
        # byte 0: 40h reading object
        # byte 1: Index LSB (index in the object dictionary)
        # byte 2: Index MSB
        # byte 3: Sub-Index
        # byte 4-7: data LSB to data MSB
#        objPCANBasic.Write();

 #       result = objPCANBasic.Read(PCAN_PCIBUS1);
  #      print result

    # cobid is the COB-ID as a string
    # snd is th message in the format ["00","bb","ef",...]
    def sendCANMsg(self,cobid,snd):
        time.sleep(0.01);
        canmsg = TPCANMsg()
        canmsg.ID = int(cobid,16)
        canmsg.LEN = 8
        canmsg.MSGTYPE = PCAN_MESSAGE_STANDARD
        for i in range(8):
            canmsg.DATA[i] = int(snd[i],16);
        
        return self.objPCANBasic.Write(self.PcanHandle, canmsg)
        

    def printPDOConf(self):
        # Do for PDO 1 to PDO 4
        for j in range(4):
            print "PDO-"+str(j)
            for k in range(3):
                result = self.sendCANMsg("603",["40",str(j),"14",str(k),"00","00","00","00"]);
                time.sleep(0.1);
                
                # The message was successfully sent
                #
                if result == PCAN_ERROR_OK:
                    # receive the response
                    result = self.objPCANBasic.Read(self.PcanHandle);
                    print "Result: " + self.formatCANMessage(result[1]);
                else:
                    # An error occurred.  We show the error.
                    #
                    print("Error: Message was NOT SENT")
            self.printPDOMapping(j);
            print ""


    def printPDOMapping(self,pdono):
        self.sendCANMsg("603",["40",str(pdono),"16","00","00","00","00","00"]);
        time.sleep(0.1);
        result = self.objPCANBasic.Read(self.PcanHandle);
#        print self.formatCANMessage(result[1]);

        numMap = result[1].DATA[4];
        for i in range(numMap):
            self.sendCANMsg("603",["40",str(pdono),"16",str(i+1),"00","00","00","00"]);                                   
            time.sleep(0.1);
            result = self.objPCANBasic.Read(self.PcanHandle);
            print self.formatCANMessage(result[1]);


    def formatCANMessage(self,msg):
        strTmp = "{0:03X}".format(msg.ID) + " "
        for i in range(msg.LEN):
            strTmp = strTmp + " " + "{0:02X}".format(msg.DATA[i])

        return strTmp


    def readAllCANMessages(self):
        time.sleep(0.1);
        result = self.objPCANBasic.Read(self.PcanHandle);
        msg = result[1];
        while msg.LEN > 0:
            print self.formatCANMessage(msg);
            result = self.objPCANBasic.Read(self.PcanHandle);
            msg = result[1];

        
            
### Loop-Functionallity  
def main():
    # Creates a PCAN-Basic application
    #
    basicExl = PCANTest()
    #basicEx1.destroy()    



if __name__ == '__main__':
    main()
