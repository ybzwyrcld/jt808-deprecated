// Copyright 2019 Yuming Meng. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/jt808_terminal_parameters.h"


uint8_t GetParameterTypeByParameterId(const uint32_t &para_id) {
  switch (para_id) {
    case GNSSPOSITIONMODE: case GNSSBAUDERATE: case GNSSOUTPUTFREQ:
    case GNSSUPLOADWAY: case STARTUPGPS: case STARTUPCDRADIO:
    case STARTUPNTRIPCORS: case STARTUPNTRIPSERV: case STARTUPJT808SERV:
    case GPSLOGGGA: case GPSLOGRMC: case GPSLOGATT:
    case CDRADIORECEIVEMODE: case CDRADIOFORMCODE: case NTRIPCORSREPORTINTERVAL:
    case NTRIPSERVICEREPORTINTERVAL: case JT808SERVICEREPORTINTERVAL:
      return kByteType;
    case CAN1UPLOADINTERVAL: case CAN2UPLOADINTERVAL: case CDRADIOWORKINGFREQ:
    case NTRIPCORSPORT: case NTRIPSERVICEPORT: case JT808SERVICEPORT:
      return kWordType;
    case HEARTBEATINTERVAL: case TCPRESPONDTIMEOUT: case TCPMSGRETRANSTIMES:
    case UDPRESPONDTIMEOUT: case UDPMSGRETRANSTIMES: case SMSRESPONDTIMEOUT:
    case SMSMSGRETRANSTIMES: case POSITIONREPORTWAY: case POSITIONREPORTPLAN:
    case NOTLOGINREPORTTIMEINTERVAL: case SLEEPREPORTTIMEINTERVAL:
    case ALARMREPORTTIMEINTERVAL: case DEFTIMEREPORTTIMEINTERVAL:
    case NOTLOGINREPORTDISTANCEINTERVAL: case SLEEPREPORTDISTANCEINTERVAL:
    case ALARMREPORTDISTANCEINTERVAL: case DEFTIMEREPORTDISTANCEINTERVAL:
    case INFLECTIONPOINTRETRANSANGLE: case ALARMSHIELDWORD: case ALARMSENDTXT:
    case ALARMSHOOTSWITCH: case ALARMSHOOTSAVEFLAGS: case ALARMKEYFLAGS:
    case MAXSPEED: case GNSSOUTPUTCOLLECTFREQ: case GNSSUPLOADSET:
    case CAN1COLLECTINTERVAL: case CAN2COLLECTINTERVAL: case CDRADIOBAUDERATE:
      return kDwordType;
    case CANSPECIALSET: case NTRIPCORSIP: case NTRIPCORSUSERNAME:
    case NTRIPCORSPASSWD: case NTRIPCORSMOUNTPOINT: case NTRIPSERVICEIP:
    case NTRIPSERVICEUSERNAME: case NTRIPSERVICEPASSWD:
    case NTRIPSERVICEMOUNTPOINT: case JT808SERVICEIP: case JT808SERVICEPHONENUM:
      return kStringType;
    default:
      return kUnknowType;
  }
}

uint16_t GetParameterLengthByParameterType(const uint8_t &para_type) {
  switch (para_type) {
    case kByteType:
      return 1;
    case kWordType:
      return 2;
    case kDwordType:
      return 4;
    case kStringType:
    case kUnknowType:
    default:
      return 0;
  }
}

