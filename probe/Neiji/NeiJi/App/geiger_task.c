#include "geiger_task.h"
#include "geiger.h"
#include "beep.h"
#include "fsy_regmap.h"
#include "cmsis_os.h"

#define GEIGER_TASK_MS  10U

static void GeigerTask(void *argument);

static osThreadId_t geigerTaskHandle;
static const osThreadAttr_t geigerTaskAttributes = {
    .name = "geigerTask",
    .stack_size = 512 * 4,
    .priority = (osPriority_t)osPriorityAboveNormal,
};

void Geiger_TaskInit(void)
{
    geigerTaskHandle = osThreadNew(GeigerTask, NULL, &geigerTaskAttributes);
}

static void GeigerTask(void *argument)
{
    (void)argument;

    Geiger_Init();

    for (;;) {
        Geiger_Doserate_Calculate();
        Dose_Rate_TH_Alarm();
        Beep_Ctr(beep_event);
        Fsy_Regmap_UpdateDoseRate(data_var.real_rate);
        osDelay(GEIGER_TASK_MS);
    }
}
