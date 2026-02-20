// SPDX-License-Identifier: GPL-2.0-only

#include <linux/kernel.h>
#include <asm/mach/arch.h>

static const char * const qcom_msm8909_compat[] __initconst = {
	"qcom,apq8009",
	"qcom,msm8909",
	NULL
};

DT_MACHINE_START(QCOM_MSM8909, "Qualcomm APQ8009")
	.dt_compat = qcom_msm8909_compat,
MACHINE_END
