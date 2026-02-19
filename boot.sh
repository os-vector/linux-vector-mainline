fastboot --cmdline 'earlycon console=ttyMSM0,115200 ro androidboot.hardware=qcom ehci-hcd.park=3 msm_rtb.filter=0x37 lpm_levels.sleep_disabled=1 rootwait' boot v-out/zImage-dtb
