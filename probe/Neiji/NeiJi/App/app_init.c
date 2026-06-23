#include "app_init.h"
#include "log.h"

void App_Init(void)
{
    Log_Init();
    Log_Info("NeiJi firmware boot");
}
