#include <stm32f1xx_ll_bus.h>
#include <stm32f1xx_ll_cortex.h>
#include <stm32f1xx_ll_pwr.h>
#include <stm32f1xx_ll_rcc.h>

#include <zephyr/zephyr.h>
#include <zephyr/pm/pm.h>

void pm_state_set(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(substate_id);

	switch (state) {
	case PM_STATE_SOFT_OFF:
		LL_PWR_EnableWakeUpPin(LL_PWR_WAKEUP_PIN1);
		LL_PWR_ClearFlag_WU();
		LL_PWR_SetPowerMode(LL_PWR_MODE_STANDBY);
		LL_LPM_EnableDeepSleep();
		k_cpu_idle();
		break;
	default:
		printk("Unsupported power state %u", state);
		break;
	}
}
