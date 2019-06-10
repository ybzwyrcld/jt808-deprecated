#include "common/jt808_terminal_parameters.h"

#include <string.h>
#include <algorithm>


bool GetNodeFromTerminalParameterListById(
         const std::list<TerminalParameter> &list,
         const uint16_t &id,
         TerminalParameter &node) {
  if (list.empty()) {
    return false;
  }

  auto it = list.begin();
  while (it != list.end()) {
    if (it->parameter_id == id) {
      node = *it;
      break;
    }
    ++it;
  }

  return (it == list.end() ? false : true);
}

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

uint8_t GetParameterLengthByParameterType(const uint8_t &para_type) {
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

void AddParameterNodeIntoList(std::list<TerminalParameter *> *para_list,
                              const uint32_t &para_id,
                              const char *para_value) {
  if (para_list == nullptr) {
      return ;
    }

  TerminalParameter *node = new TerminalParameter;
  memset(node, 0x0, sizeof(*node));
  node->parameter_id = para_id;
  node->parameter_type = GetParameterTypeByParameterId(para_id);
  node->parameter_len = GetParameterLengthByParameterType(
      node->parameter_type);
  if (para_value != nullptr) {
    if (node->parameter_type == kStringType) {
      node->parameter_len = strlen(para_value);
    }
    memcpy(node->parameter_value, para_value, node->parameter_len);
  }
  para_list->push_back(node);
}

void PrepareParemeterIdList(std::vector<std::string> &va_vec,
                            std::vector<uint32_t> &id_vec) {
  char parameter_id[9] = {0};
  std::string arg;
  reverse(id_vec.begin(), id_vec.end());
  while (!id_vec.empty()) {
    snprintf(parameter_id, 5, "%04X", id_vec.back());
    arg = parameter_id;
    va_vec.push_back(arg);
    memset(parameter_id, 0x0, sizeof(parameter_id));
    id_vec.pop_back();
  }
  reverse(va_vec.begin(), va_vec.end());
}

