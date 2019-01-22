/*
  aSysBus CAN definitions

  @copyright 2015-2017 Florian Knodt, www.adlerweb.info

  Based on iSysBus - 2010 Patrick Amrhein, www.isysbus.org

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

#ifndef ASB_CAN__H
#define ASB_CAN__H
    #include "asb.h"
    #include <mcp_can.h>
    #include <SPI.h>

/**
* If the same message arrives within DUPLICATE_TIMEOUT (in ms), it is considered a duplicate and filtered out (if enabled)
*/
#define DUPLICATE_TIMEOUT 20
	
    /**
     * Global variable indicating a CAN-bus got a message
     * This is currently shared amongst all CAN-interfaces!
     */
    extern volatile bool asb_CANIntReq;

    /**
     * Global interrupt function called on CAN interrupts
     * This is currently shared amongst all CAN-interfaces!
     */
    void asb_CANInt(void);

    /**
     * CAN Communication Interface
     * @see ASB_COMM
     */
    class ASB_CAN : public ASB_COMM {
        private:
            /**
             * CAN-Bus object
             * @see MCP_CAN
             * @see https://github.com/Seeed-Studio/CAN_BUS_Shield
             */
            MCP_CAN _interface;

            /**
             * Interrupt pin to be used
             */
            byte _intPin = 0;

            /**
             * CAN Bus Speed
             */
            byte _speed = 0;

            /**
             * CAN Crystal Frequency
             */
            byte _clockspd = 0;
			
            /**
             * Is the standby pin of the transceiver connected to the MCP2515?
             */
            bool _transceiverStandbyMCP2515 = true;
						
			/**
             * Number of the pin connected to the transceiver standby
             */
            int _transceiverStandbyPin = -1;

			/**
             * CAN AutoSleep
             */
            bool _autosleep = false;
			
			/**
			 * Time of last activity on the can bus (used for autosleep)
			 */
			 unsigned long _lastBusActivity = millis();
			 
			 /**
			 * Keep awake time after last message (in ms, used for autosleep)
			 */
			 unsigned int _keepAwakeTime = 200;
			 
			 /**
             * Send wakeup before each message
             */
            bool _sendWakeup = false;
			
			/**
             * Pause between wakeup and msg to let the receiving node wake up properly (in ms)
             */
            unsigned int _wakeupWaitTime = 100;
			
			/*
			* Enable or disable duplicate filtering
			*/
			bool _filterDuplicates = false;
			
			/**
             * _lastId, _lastLen, _lastBuf and _lastMsgTime are used to check for duplicate messages
             */
			unsigned long _lastId;
			int _lastLen = -1;
			unsigned char _lastBuf[8];
			unsigned long _lastMsgTime = 0;
			
        public:
            /**
             * Constructor for NonInterrupt operation
             * @TODO This is not implemented yet…
             * @param cs pin used for CHIP_SELECT
             * @param speed CAN bus speed definition from mcp_can_dfs.h (bottom)
             * @param clockspd MCP crystal frequency from mcp_can_dfs.h (bootom)
             * @see https://github.com/Seeed-Studio/CAN_BUS_Shield/blob/master/mcp_can_dfs.h
             */
            ASB_CAN(byte cs, byte speed, byte clockspd);

            /**
             * Constructor for Interrupt based operation
             * @param cs pin used for CHIP_SELECT
             * @param speed CAN bus speed definition from mcp_can_dfs.h (bottom)
             * @param clockspd MCP crystal frequency from mcp_can_dfs.h (bootom)
             * @param interrupt pin used for interrupt. Must have hardware INT support
             * @see https://github.com/Seeed-Studio/CAN_BUS_Shield/blob/master/mcp_can_dfs.h
             */
            ASB_CAN(byte cs, byte speed, byte clockspd, byte interrupt);

            /**
             * Initialize CAN controller
             * @return byte error code
             * @see https://github.com/Seeed-Studio/CAN_BUS_Shield/blob/master/mcp_can_dfs.h
             */
            byte begin(void);

			/**
             * AutoSleep: put the CAN controller into sleep mode between sending/receiving
			 * @param doSleep enable/disable AutoSleep
			 * @param keepAwakeTime How long should the controller stay awake after last bus activity (default: 400ms)? This must be significantly longer than the "wakeupWaitTime" in "setSendWakeup" (see below)
             * @return byte error code
             * @see https://github.com/Seeed-Studio/CAN_BUS_Shield/blob/master/mcp_can_dfs.h
             */
            byte setAutoSleep(bool doSleep, long keepAwakeTime = 400);
			
			/**
             * Wakeup on receiving messages
			 * @param doWakeup true: enable, false: disable
             */
            void setSleepWakeup(bool doWakeup);

			/**
             * The Rs (standby) pin of the transceiver (mcp2551 or similar) can either be connected to an Arduino output (isMCP2515pin = false) or to the RX0BF/RX1BF pin of the mcp2515 (isMCP2515pin = true). If pin is greater than -1, the transceiver will be put to sleep/standby together with the mcp2515.
			 Use only after asbCan is already initialized + started!
			 * @param isMCP2515pin true: connected to MCP2515, false: connected to Arduino
			 * @param pin -1: disable standby of transceiver, 0-99: pin number
             */
            void setTransceiverStandbyPin(bool isMCP2515pin, int pin);					
			
			/**
             * sleep: put the CAN controller (and transceiver) into sleep mode
			 * @return byte error code
             * @see https://github.com/Seeed-Studio/CAN_BUS_Shield/blob/master/mcp_can_dfs.h
             */
            byte sleep();

			/**
             * wake: wake the CAN controller (and transceiver) manually from sleep mode
			 * @return byte error code
             * @see https://github.com/Seeed-Studio/CAN_BUS_Shield/blob/master/mcp_can_dfs.h
             */
            byte wake();
			
			/**
             * Check if MCP2515 is sleeping
             * @return true if MCP2515 is in MODE_SLEEP
             */
			 bool isSleeping();
			
            /**
             * SendWakeup: send a wakeup message to the receiving node before each normal message
			 * @param doSendWakeup enable/disable SendWakeup
             * @param wakeupWaitTime: pause between wakeup and msg to let the receiving node wake up properly (in ms). The default of 200ms should give you some safety margin (tested with MCP2515 + MCP2515 @125kps).
             */
            void setSendWakeup(bool doSendWakeup, unsigned int wakeupWaitTime = 200);
			
			/**
             * setfilterDuplicates: If enabled, duplicate messages that arrive within a small time frame (default: 20ms) are ignored. Duplicate messages are likely in a network where most nodes are sleeping. When the MCP2515 wakes up it enters LISTENONLY mode and does not send ACKs, so the transmitter will retransmit the same message a few times until at least one other node on the net is fully woken up.
			 * @param enable enable/disable duplicate filtering
             */
			void setFilterDuplicates(bool enabled);
			/**
             * Parse CAN-address into our metadata format
             * @param canAddr CAN-address
             * @return asbMeta object containing decoded metadata, targst/source==0x00 on errors
             */
			 asbMeta asbCanAddrParse(unsigned long canAddr);

            /**
             * Assemble a CAN-address based on our adressing format
             * @param meta asbMeta object
             * @return unsigned long CAN-address
             */
            unsigned long asbCanAddrAssemble(asbMeta meta);
            /**
             * Assemble a CAN-address based on our adressing format
             * @param type 2 bit message type (ASB_PKGTYPE_*)
             * @param target address between 0x0001 and 0x07FF/0xFFFF
             * @param source source address between 0x0001 and 0x07FF
             * @return unsigned long CAN-address
             */
            unsigned long asbCanAddrAssemble(byte type, unsigned int target, unsigned int source);
            /**
             * Assemble a CAN-address based on our adressing format
             * @param type 2 bit message type (ASB_PKGTYPE_*)
             * @param target address between 0x0001 and 0x07FF/0xFFFF
             * @param source source address between 0x0001 and 0x07FF
             * @param port port address between 0x00 and 0x1F, Unicast only
             * @return unsigned long CAN-address
             */
            unsigned long asbCanAddrAssemble(byte type, unsigned int target, unsigned int source, char port);

            /**
             * Send message to CAN-bus
             * @param type 2 bit message type (ASB_PKGTYPE_*)
             * @param target address between 0x0001 and 0x07FF/0xFFFF
             * @param source source address between 0x0001 and 0x07FF
             * @param port port address between 0x00 and 0x1F, Unicast only
             * @param len number of bytes to send (0-8)
             * @param data array of bytes to send
             */
            bool asbSend(byte type, unsigned int target, unsigned int source, char port, byte len, const byte *data);

            /**
             * Receive a message from the CAN-bus
             *
             * This polls the CAN-Controller for new messages.
             * The received message will be passed to &pkg, if no message
             * is available the function will return false.
             *
             * @param pkg asbPacket-Reference to store received packet
             * @return true if a message was received
             */
            bool asbReceive(asbPacket &pkg);
    };

#endif /* ASB_CAN__H */
