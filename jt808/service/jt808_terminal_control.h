#ifndef JT808_SERVICE_JT808_TERMINAL_CONTROL_H_
#define JT808_SERVICE_JT808_TERMINAL_CONTROL_H_


#define WIRELESSUPGRADE                     0x01
#define CONNECTSPECIALSERVER                0x02
#define TERMINALPOWEROFF                    0x03
#define TERMINALREBOOT                      0x04
#define TERMINALRESET                       0x05
#define TURNOFFDATACOMMUNICATION            0x06
#define TURNOFFALLWIRELESSCOMMUNICATION     0x07

enum TerminalControlType {
  kUnknowControlType = 0x0,
  kWirelessUpgrade,
  kConnectSpecialServer,
  kTerminalPowerOff,
  kTerminalReboot,
  kTerminalReset,
  kTurnOffDataCommunication,
  kTurnOffAllWirelessCommunication,
};

#endif  // JT808_SERVICE_JT808_TERMINAL_CONTROL_H_
