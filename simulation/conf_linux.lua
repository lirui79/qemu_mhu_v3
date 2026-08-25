-- Linux A76 variant.  Reuse the validated R52/A76 platform and override only
-- the A76 payload plus interrupt connections needed by Linux.

local source = debug.getinfo(1, "S").source:sub(2)
local directory = source:match("(.*/)") or "./"

dofile(directory .. "conf.lua")

platform.ddr_0.load.bin_file = directory .. "../linux_a76/build/linux_payload.bin"

platform.pl011_uart_2.irq = {
    bind = "&plugin_1.target_signal_socket_2",
}

platform.plugin_1.target_signals_num = 3
platform.plugin_1.plugin_pass.initiator_signals_num = 3
platform.plugin_1.plugin_pass.initiator_signal_socket_2 = {
    bind = "&gic_0.spi_in_18",
}

local a76 = platform.plugin_1.cpu_0
a76.irq_timer_sec_out  = {bind = "&gic_0.ppi_in_cpu_0_29"}
a76.irq_timer_phys_out = {bind = "&gic_0.ppi_in_cpu_0_30"}
a76.irq_timer_virt_out = {bind = "&gic_0.ppi_in_cpu_0_27"}
a76.irq_timer_hyp_out  = {bind = "&gic_0.ppi_in_cpu_0_26"}
