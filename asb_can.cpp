/**
  aSysBus CAN interface

  @copyright 2015-2017 Florian Knodt, www.adlerweb.info

  Based on iSysBus - 2010 Patrick Amrhein, www.isysbus.org

  This interface depends on the CAN_BUS_Shield library.
    Download: https://github.com/Seeed-Studio/CAN_BUS_Shield

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ASB_CAN__C
#define ASB_CAN__C
    #include "asb_can.h"
    #include "asb_proto.h"
    #include <mcp_can.h>
    #include <SPI.h>

    volatile bool asb_CANIntReq=false;

    void asb_CANInt() {
      asb_CANIntReq=true;
    }

    /*
     * @TODO communication without INT
     */
    /*ASB_CAN::ASB_CAN(byte cs, byte speed, byte clock) {
        ASB_CAN::interface = MCP_CAN(cs);
        lastErr = ASB_CAN::interface.begin(MCP_EXT, speed, clock);
        if(lastErr == CAN_OK) ASB_CAN::interface.setMode(MODE_NORMAL);
        wakelock = 1;
        #error Communication without interrups isn't implemented yet
    }*/


    ASB_CAN::ASB_CAN(byte cs, byte speed, byte clockspd, byte interrupt) :
    _interface(cs) {
        pinMode(interrupt, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(interrupt), asb_CANInt, FALLING);
        _intPin = interrupt;
        _speed = speed;
        _clockspd = clockspd;
        //Begin doesn't work here!
    }

    byte ASB_CAN::begin() {
        lastErr = _interface.begin(_speed, _clockspd);
        return lastErr;
    }
	
	byte ASB_CAN::setAutoSleep(bool doSleep, long keepAwakeTime) {
		_autosleep = doSleep;
		_keepAwakeTime = keepAwakeTime;
		if(_autosleep && (_interface.checkReceive() != CAN_MSGAVAIL) && (millis() > _lastBusActivity + _keepAwakeTime)) {
			return sleep();
		} else {
			return CAN_OK;
		}	
	}
	
	void ASB_CAN::setSleepWakeup(bool doWakeup) {
		_interface.setSleepWakeup(doWakeup);
	}
	
    void ASB_CAN::setTransceiverStandbyPin(bool isMCP2515pin, int pin) {
		_transceiverStandbyMCP2515 = isMCP2515pin;
		_transceiverStandbyPin = pin;
	
		if(_transceiverStandbyPin > -1) {
			// config pin as output
			if(_transceiverStandbyMCP2515) {
				_interface.mcpPinMode(_transceiverStandbyPin, MCP_PIN_OUT);
			} else {
				pinMode(_transceiverStandbyPin, OUTPUT);
			}
		
			// set standby/running
			if(_transceiverStandbyMCP2515) {
				_interface.mcpDigitalWrite(_transceiverStandbyPin, (_interface.getMode() == MODE_SLEEP));
			} else {
				digitalWrite(_transceiverStandbyPin, (_interface.getMode() == MODE_SLEEP));
			}
		}
	}	
	
	byte ASB_CAN::sleep() {
		byte res;
		
		if(!isSleeping()) {
			#ifdef ASB_DEBUG
                Serial.print(F("Going to sleep...")); Serial.println(); Serial.flush();
            #endif
		
			// Put MCP2515 into sleep mode
			res = _interface.sleep();
		
			// Put transceiver into standby
			if((res == MCP2515_OK) && (_transceiverStandbyPin > -1)) {
				if(_transceiverStandbyMCP2515) {
					_interface.mcpDigitalWrite(_transceiverStandbyPin, HIGH);
				} else {
					digitalWrite(_transceiverStandbyPin, HIGH);
				}
			}
			return res;
		} else
			return MCP2515_OK;
	}
	
	byte ASB_CAN::wake() {
		byte res;
		
		// start the keep awake timer
		_lastBusActivity = millis();
		
		#ifdef ASB_DEBUG
			Serial.print(F("Waking up if necessary...")); Serial.println(); Serial.flush();
        #endif
		
		// wake MCP2515
		res = _interface.wake();
		
		// wake transceiver
		if((res == MCP2515_OK) && (_transceiverStandbyPin > -1)) {
			if(_transceiverStandbyMCP2515) {
				_interface.mcpDigitalWrite(_transceiverStandbyPin, LOW);
			} else {
				digitalWrite(_transceiverStandbyPin, LOW);
			}
		}
		return res;
	}
	
	bool ASB_CAN::isSleeping() {
		return _interface.getMode() == MODE_SLEEP;
	}
	
	void ASB_CAN::setSendWakeup(bool doSendWakeup, unsigned int wakeupWaitTime) {
		_sendWakeup = doSendWakeup;
		_wakeupWaitTime = wakeupWaitTime;
	}
	
	void ASB_CAN::setFilterDuplicates(bool enabled) {
		_filterDuplicates = enabled;
	}

    asbMeta ASB_CAN::asbCanAddrParse(unsigned long canAddr) {
        asbMeta temp;

        temp.target = 0;
        temp.source = 0;
        temp.port = -1;

        temp.type = ((canAddr >> 28) & 0x03);
        temp.source = (canAddr & 0x7FF);
        temp.target = ((canAddr >> 11) & 0xFFFF);

        if(temp.type == 0x03) { //Unicast
            temp.port = ((canAddr >> 23) & 0x1F);
            temp.target &= 0x7FF;
        }

        return temp;
    }

    unsigned long ASB_CAN::asbCanAddrAssemble(asbMeta meta) {
        return asbCanAddrAssemble(meta.type, meta.target, meta.source, meta.port);
    }

    unsigned long ASB_CAN::asbCanAddrAssemble(byte type, unsigned int target, unsigned int source) {
        return asbCanAddrAssemble(type, target, source, -1);
    }

    unsigned long ASB_CAN::asbCanAddrAssemble(byte type, unsigned int target, unsigned int source, char port) {
          unsigned long addr = 0x80000000;

          if(type > 0x03) return 0;
          addr |= ((unsigned long)type << 28);

          if(type == ASB_PKGTYPE_UNICAST) {
            if(target > 0x7FF) return 0;
            if(port < 0 ||port > 0x1F) return 0;

            addr |= ((unsigned long)port << 23);
          }else{
            if(target > 0xFFFF) return 0;
          }

          addr |= ((unsigned long)target << 11);

          if(source > 0x7FF) return 0;
          addr |= source;

          return addr;
    }

    bool ASB_CAN::asbSend(byte type, unsigned int target, unsigned int source, char port, byte len, const byte *data) {
        unsigned long addr = asbCanAddrAssemble(type, target, source, port);
        if(addr == 0) return false;
		 
        wake(); // make sure the node is running properly
		
		// send:
		if(_sendWakeup) {
			byte wakedata[1] = {ASB_CMD_WAKE};
			lastErr = _interface.sendMsgBuf(addr, 1, 1, wakedata);
			if(lastErr != CAN_OK) {
			    #ifdef ASB_DEBUG
					Serial.print(F("Error sending wakeup message")); Serial.println(); Serial.flush();
				#endif
				return false;
			}
			delay(_wakeupWaitTime);
		}
		lastErr = _interface.sendMsgBuf(addr, 1, len, data);
        
		// autosleep if necessary
		if(_autosleep && (millis() > (_lastBusActivity + _keepAwakeTime)))
			sleep();
		
		if(lastErr != CAN_OK) return false;
        return true;
    }

    bool ASB_CAN::asbReceive(asbPacket &pkg) {

        unsigned long rxId;
        byte len = 0;
        byte rxBuf[8];
		byte res;
		
		if(asb_CANIntReq) { // wake on any activity on the bus, even if no complete message was received
			// If the nodes wakes automatically from an interrupt, it will still be in a 'half sleeping' state 
			// (transceiver in standby, MCP2515 in listenonly mode). We have to call "wake()" to insure the node
			// is running properly.
			wake();
			// reset interrupt notification
			asb_CANIntReq = (digitalRead(_intPin) == LOW);
		}

        if(_interface.checkReceive() != CAN_MSGAVAIL) {
			// no message available, check for autosleep
			if(_autosleep && (millis() > (_lastBusActivity + _keepAwakeTime)))
				sleep();
			return false;
		} else {
			byte state = _interface.readMsgBufID(&rxId, &len, rxBuf);

			if(state != CAN_OK) return false;
			
			// check if this is a duplicate message (including a timeout)
            if(!_filterDuplicates || (rxId != _lastId) || (len != _lastLen) || (millis() > _lastMsgTime + DUPLICATE_TIMEOUT) || (memcmp((const void *)_lastBuf, (const void *)rxBuf, sizeof(rxBuf)) != 0))
            {
				if(_filterDuplicates) {
					_lastId = rxId;
					_lastLen = len;
					memcpy(_lastBuf, rxBuf, sizeof(rxBuf));
					_lastMsgTime = millis();
				}

				pkg.meta = asbCanAddrParse(rxId);
				pkg.len = len;

				for(byte i=0; i<len; i++) pkg.data[i] = rxBuf[i];

				return true;
			} else { // was duplicate
				#ifdef ASB_DEBUG
					Serial.print(F("Duplicate message ignored.")); Serial.println(); Serial.flush();
				#endif
				return false;
			}
		}
    }


#endif /* ASB_CAN__C */
