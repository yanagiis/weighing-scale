#include <zephyr/device.h>
#include <zephyr/pm/pm.h>
#include <zephyr/zephyr.h>

#include <stm32f1xx_ll_bus.h>
#include <stm32f1xx_ll_cortex.h>
#include <stm32f1xx_ll_pwr.h>
#include <stm32f1xx_ll_rcc.h>

void pm_state_set(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(substate_id);

	switch (state) {
	case PM_STATE_SOFT_OFF:
		LL_PWR_DisableBkUpAccess();
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

/* Initialize STM32 Power */
static int stm32_pm_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	/* enable Power clock */
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

#ifdef CONFIG_DEBUG
	/* Enable the Debug Module during STOP mode */
	LL_DBGMCU_EnableDBGStopMode();
#endif /* CONFIG_DEBUG */

	return 0;
}

SYS_INIT(stm32_pm_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
