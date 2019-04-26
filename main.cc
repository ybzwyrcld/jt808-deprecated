#include "service/jt808_service.h"

int main(int argc, char *argv[])
{
  Jt808Service my_service;
  my_service.Init(3398, 10);
  my_service.Run(2000);

  return  0;
}

