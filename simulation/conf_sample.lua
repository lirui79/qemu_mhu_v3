-- Virtual platform configuration for Cortex-R52 + Cortex-A76 with MHU-DavarAE
-- Inter-processor communication via MHU doorbell/fast channels.
-- Address map: see fw/cortex-r52/addr_map.h, fw/cortex-a76/addr_map.h

function top()
    local str = debug.getinfo(2, "S").source:sub(2)
    if str:match("(.*/)")
    then
        return str:match("(.*/)")
    else
        return "./"
    end
end

EXECUTABLE_PATH = GET("executable_path")

if EXECUTABLE_PATH == nil then
    print("Error: executable_path is not set")
end

function formatExecutable(executable)
    local isWindows = package.config:sub(1,1) == '\\'
    if isWindows then
        return "\\" .. executable .. ".exe"
    else
        return "/" .. executable
    end
end

-- Execution file
EXEC_FILE_R52 = "./fw/cortex-r52/cortex-r52.bin"
EXEC_FILE_A76 = "./fw/cortex-a76/cortex-a76.bin"

-- Dual-core Cortex-R52 cluster
local R52_NUM_CPUS          = 2
local GICV3_REDIST_SIZE     = 0x00020000
local ARCH_TIMER_NS_EL1_IRQ = 16 + 14   -- PPI 30

-- Memory map constants
local RAM_BASE           = 0x00000000
local RAM_SIZE           = 0x00020000
local DDR_BASE           = 0x80000000
local DDR_SIZE           = 0x20000000   -- 512 MB
local GICD_BASE_R52      = 0x2F000000
local GICD_SIZE          = 0x00010000
local GICR_BASE_R52      = 0x2F100000
local GICR_SIZE_R52      = R52_NUM_CPUS * GICV3_REDIST_SIZE  -- 256 KiB for 2 CPUs

-- A76 GIC (separate RemotePass, addresses local to its router)
local GICD_BASE_A76      = 0x2F020000
local GICD_SIZE_A76      = 0x00010000
local GICR_BASE_A76      = 0x2F120000
local GICR_SIZE_A76      = 0x00020000   -- 1 CPU × 0x20000

-- master_if_* window @ 0x2FC6_0000 (SoC address map)
local UART0_BASE         = 0x2FC60000
local UART0_SIZE         = 0x00001000
local UART1_BASE         = 0x2FC61000
local UART1_SIZE         = 0x00001000
local TS_GEN_BASE        = 0x2FC62000   -- master_if_timestamp_gen (apb_ctrl)
local TS_GEN_SIZE        = 0x00001000
local TS_GEN_REG_BASE    = 0x2FC63000   -- master_if_timestamp_gen_reg (apb_cnt)
local TS_GEN_REG_SIZE    = 0x00001000
local DMA_BASE           = 0x2FC64000   -- master_if_dma, 16 KiB
local DMA_SIZE           = 0x00004000

-- MHU SoC map (inter.md §3.4): snd/rec each = data(64KB) + reg(128KB)
--   snd_data 0x2FC00000..0x2FC0FFFF  snd_reg 0x2FC10000..0x2FC2FFFF
--   rec_data 0x2FC30000..0x2FC3FFFF  rec_reg 0x2FC40000..0x2FC5FFFF
-- Model uses 64 KiB PBX/MBX frames packed into the *_reg windows (2 × 64 KiB).
local MHU_SND_DATA       = 0x2FC00000
local MHU_SND_DATA_SIZE  = 0x00010000   -- 64 KB
local MHU_SND_REG        = 0x2FC10000
local MHU_SND_REG_SIZE   = 0x00020000   -- 128 KB
local MHU_REC_DATA       = 0x2FC30000
local MHU_REC_DATA_SIZE  = 0x00010000   -- 64 KB
local MHU_REC_REG        = 0x2FC40000
local MHU_REC_REG_SIZE   = 0x00020000   -- 128 KB
local MHU_BLOCK_SIZE     = 0x00010000   -- 64 KiB per PBX/MBX frame
local MHU_R52_SENDER     = MHU_SND_REG                          -- PBX @ snd_reg[0]
local MHU_A76_SENDER     = MHU_SND_REG + MHU_BLOCK_SIZE         -- PBX @ snd_reg[1]
local MHU_R52_RECEIVER   = MHU_REC_REG                          -- MBX @ rec_reg[0]
local MHU_A76_RECEIVER   = MHU_REC_REG + MHU_BLOCK_SIZE         -- MBX @ rec_reg[1]

-- VP-only IRQ injector
local IRQ_TEST_BASE      = 0xC0001000

platform = {
    moduletype = "ContainerDeferModulesConstruct";
    quantum_ns = 10000000;

    router = {
        moduletype = "router";
        log_level = 0;
    },

    -- Shared RAM: R52 firmware loaded at 0x0
    ram_0 = {
        moduletype = "gs_memory",
        target_socket = {address = RAM_BASE, size = RAM_SIZE, bind = "&router.initiator_socket"},
        shared_memory = true,
        load = {bin_file = top() .. EXEC_FILE_R52, offset = 0},
    },

    -- Shared DDR: A76 firmware loaded at DDR_BASE + 0x100000
    ddr_0 = {
        moduletype = "gs_memory",
        target_socket = {address = DDR_BASE, size = DDR_SIZE, bind = "&router.initiator_socket"},
        shared_memory = true,
        load = {bin_file = top() .. EXEC_FILE_A76, offset = 0x100000},
    },

    keep_alive_0 = { moduletype = "keep_alive" },

    charbackend_stdio_0 = {
        moduletype = "char_backend_stdio";
        read_write = true;
        ansi_highlight = "";
    };

    -- Console UART (master_if_uart_0 @ 0x2FC60000), SPI 0 -> R52 GIC INTID 32
    pl011_uart_0 = {
        moduletype = "Pl011",
        dylib_path = "uart-pl011",
        target_socket = {address = UART0_BASE, size = UART0_SIZE, bind = "&router.initiator_socket"},
        irq = {bind = "&plugin_0.target_signal_socket_0"},
        backend_socket = {bind = "&charbackend_stdio_0.biflow_socket"},
    },

    -- UART1 placeholder
    uart_1_mem = {
        moduletype = "gs_memory",
        target_socket = {address = UART1_BASE, size = UART1_SIZE, bind = "&router.initiator_socket"},
        shared_memory = true,
    },

    -- System counter
    timestamp_gen_0 = {
        moduletype = "timestamp_gen",
        dylib_path = "timestamp_gen",
        freq_hz = 62500000,
        apb_ctrl = {address = TS_GEN_BASE, size = TS_GEN_SIZE, bind = "&router.initiator_socket"},
        apb_cnt = {address = TS_GEN_REG_BASE, size = TS_GEN_REG_SIZE, bind = "&router.initiator_socket"},
    },

    -- DMAC (combined + per-channel + common-register IRQs)
    -- Model supports up to 8 channels; irq_ch0..irq_ch7 are per-channel,
    -- irq_cmn is common-register.
    dma_0 = {
        moduletype = "dw_axi_dmac",
        dylib_path = "dw_axi_dmac",
        num_channels = 4,
        target_socket = {address = DMA_BASE, size = DMA_SIZE, bind = "&router.initiator_socket"},
        initiator_socket = {bind = "&router.target_socket"},
        irq     = {bind = "&plugin_0.target_signal_socket_2"},   -- dma_intr     (combined)
        irq_ch0 = {bind = "&plugin_0.target_signal_socket_5"},   -- SPI[1]  ch0
        irq_ch1 = {bind = "&plugin_0.target_signal_socket_6"},   -- SPI[2]  ch1
        irq_ch2 = {bind = "&plugin_0.target_signal_socket_7"},   -- SPI[3]  ch2
        irq_ch3 = {bind = "&plugin_0.target_signal_socket_8"},   -- SPI[4]  ch3
        irq_ch4 = {bind = "&plugin_0.target_signal_socket_9"},   -- SPI[5]  ch4
        irq_ch5 = {bind = "&plugin_0.target_signal_socket_10"},  -- SPI[6]  ch5
        irq_ch6 = {bind = "&plugin_0.target_signal_socket_11"},  -- SPI[7]  ch6
        irq_ch7 = {bind = "&plugin_0.target_signal_socket_12"},  -- SPI[8]  ch7
        irq_cmn = {bind = "&plugin_0.target_signal_socket_13"},  -- SPI[17] cmn
    },

    -- ============================================================
    -- MHU-DavarAE: PBX/MBX regs + SoC snd_data/rec_data SRAM
    -- ============================================================
    mhu_0 = {
        moduletype = "mhu_davarae",
        dylib_path = "mhu_davarae",
        snd_data     = {address = MHU_SND_DATA,     size = MHU_SND_DATA_SIZE, bind = "&router.initiator_socket"},
        rec_data     = {address = MHU_REC_DATA,     size = MHU_REC_DATA_SIZE, bind = "&router.initiator_socket"},
        r52_sender   = {address = MHU_R52_SENDER,   size = MHU_BLOCK_SIZE, bind = "&router.initiator_socket"},
        r52_receiver = {address = MHU_R52_RECEIVER, size = MHU_BLOCK_SIZE, bind = "&router.initiator_socket"},
        a76_sender   = {address = MHU_A76_SENDER,   size = MHU_BLOCK_SIZE, bind = "&router.initiator_socket"},
        a76_receiver = {address = MHU_A76_RECEIVER, size = MHU_BLOCK_SIZE, bind = "&router.initiator_socket"},
        -- Interrupt routing:
        --   R52 MHU receiver (MBX) → plugin_0.target_signal_socket_3 → R52 GIC spi_in_78  (mhur_mbx_int, INT_ID 110)
        --   R52 MHU sender   (PBX) → plugin_0.target_signal_socket_4 → R52 GIC spi_in_46  (mhus_pbx_int,  INT_ID  78)
        --   A76 MHU receiver (MBX) → plugin_1.target_signal_socket_0 → A76 GIC spi_in_78  (mhur_mbx_int, INT_ID 110)
        --   A76 MHU sender   (PBX) → plugin_1.target_signal_socket_1 → A76 GIC spi_in_46  (mhus_pbx_int,  INT_ID  78)
        irq_r52 = {bind = "&plugin_0.target_signal_socket_3"},
        irq_r52_sender = {bind = "&plugin_0.target_signal_socket_4"},
        irq_a76 = {bind = "&plugin_1.target_signal_socket_0"},
        irq_a76_sender = {bind = "&plugin_1.target_signal_socket_1"},
    },

    irq_generator = {
        target_socket = {address = IRQ_TEST_BASE, size = 0x1000, bind = "&router.initiator_socket"},
        spi_test = {bind = "&plugin_0.target_signal_socket_1"},
    },

    -- ============================================================
    -- R52 RemotePass (remote_cpu_r52)
    -- ============================================================
    plugin_0 = {
        moduletype = "RemotePass",
        exec_path = EXECUTABLE_PATH .. formatExecutable("remote_cpu_r52_mhu"),
        remote_argv = {"--param", "log_level=1"},
        tlm_initiator_ports_num = 1,
        tlm_target_ports_num = 0,
        target_signals_num = 14,     -- UART(0) IRQtest(1) DMA(2) MHU_rx(3) MHU_tx(4) DMA_ch[0..7](5..12) DMA_cmn(13)
        initiator_signals_num = 0,
        initiator_socket_0 = {bind = "&router.target_socket"},

        plugin_pass = {
            moduletype = "RemotePass",
            tlm_initiator_ports_num = 0,
            tlm_target_ports_num = 1,
            target_signals_num = 0,
            initiator_signals_num = 14,
            target_socket_0 = {
                address = RAM_BASE,
                size = 0x100000000,
                priority = 100,
                bind = "&router.initiator_socket",
            },
            initiator_signal_socket_0  = {bind = "&gic_0.spi_in_18"},  -- UART        (INT_ID 50 = SPI[18])
            initiator_signal_socket_1  = {bind = "&gic_0.spi_in_19"},  -- IRQ test    (INT_ID 51 = SPI[19])
            initiator_signal_socket_2  = {bind = "&gic_0.spi_in_0"},   -- DMA dma_intr(INT_ID 32 = SPI[0])
            initiator_signal_socket_3  = {bind = "&gic_0.spi_in_78"},  -- MHU mbx     (INT_ID 110 = SPI[78])
            initiator_signal_socket_4  = {bind = "&gic_0.spi_in_46"},  -- MHU pbx     (INT_ID 78 = SPI[46])
            initiator_signal_socket_5  = {bind = "&gic_0.spi_in_1"},   -- DMA ch0     (INT_ID 33 = SPI[1])
            initiator_signal_socket_6  = {bind = "&gic_0.spi_in_2"},   -- DMA ch1     (INT_ID 34 = SPI[2])
            initiator_signal_socket_7  = {bind = "&gic_0.spi_in_3"},   -- DMA ch2     (INT_ID 35 = SPI[3])
            initiator_signal_socket_8  = {bind = "&gic_0.spi_in_4"},   -- DMA ch3     (INT_ID 36 = SPI[4])
            initiator_signal_socket_9  = {bind = "&gic_0.spi_in_5"},   -- DMA ch4     (INT_ID 37 = SPI[5])
            initiator_signal_socket_10 = {bind = "&gic_0.spi_in_6"},   -- DMA ch5     (INT_ID 38 = SPI[6])
            initiator_signal_socket_11 = {bind = "&gic_0.spi_in_7"},   -- DMA ch6     (INT_ID 39 = SPI[7])
            initiator_signal_socket_12 = {bind = "&gic_0.spi_in_8"},   -- DMA ch7     (INT_ID 40 = SPI[8])
            initiator_signal_socket_13 = {bind = "&gic_0.spi_in_17"},  -- DMA cmnreq  (INT_ID 49 = SPI[17])
        },

        qemu_inst_mgr = { moduletype = "QemuInstanceManager" },
        qemu_inst = {
            moduletype = "QemuInstance",
            args = {"&qemu_inst_mgr", "AARCH64"},
            sync_policy = "multithread-freerunning",
        },

        router = { moduletype = "router", log_level = 0 },

        global_peripheral_initiator_arm_0 = {
            moduletype = "global_peripheral_initiator",
            args = {"&qemu_inst", "&cpu_0"},
            global_initiator = {bind = "&router.target_socket"},
        },

        gic_0 = {
            moduletype = "arm_gicv3",
            args = {"&qemu_inst"},
            dylib_path = "arm_gicv3",
            dist_iface = {address = GICD_BASE_R52, size = GICD_SIZE, bind = "&router.initiator_socket"},
            redist_iface_0 = {address = GICR_BASE_R52, size = GICR_SIZE_R52, bind = "&router.initiator_socket"},
            num_cpus = R52_NUM_CPUS,
            num_spi = 96,
            redist_region = {R52_NUM_CPUS},
            irq_out_0 = {bind = "&cpu_0.irq_in"},
            fiq_out_0 = {bind = "&cpu_0.fiq_in"},
            virq_out_0 = {bind = "&cpu_0.virq_in"},
            vfiq_out_0 = {bind = "&cpu_0.vfiq_in"},
            irq_out_1 = {bind = "&cpu_1.irq_in"},
            fiq_out_1 = {bind = "&cpu_1.fiq_in"},
            virq_out_1 = {bind = "&cpu_1.virq_in"},
            vfiq_out_1 = {bind = "&cpu_1.vfiq_in"},
        },

        cpu_0 = {
            moduletype = "cpu_arm_cortexR52",
            args = {"&qemu_inst"},
            dylib_path = "cpu_arm_cortexR52",
            mem = {bind = "&router.target_socket"},
            rvbar = RAM_BASE,
            has_el2 = false,
            psci_conduit = "disabled",
            mp_affinity = 0,
            start_powered_off = false,
            irq_timer_phys_out = {bind = "&gic_0.ppi_in_cpu_0_30"},
        },

        cpu_1 = {
            moduletype = "cpu_arm_cortexR52",
            args = {"&qemu_inst"},
            dylib_path = "cpu_arm_cortexR52",
            mem = {bind = "&router.target_socket"},
            rvbar = RAM_BASE,
            has_el2 = false,
            psci_conduit = "disabled",
            mp_affinity = 1,
            start_powered_off = false,
            irq_timer_phys_out = {bind = "&gic_0.ppi_in_cpu_1_30"},
        },
    },

    -- ============================================================
    -- A76 RemotePass (remote_cpu_a76) — single A76 core
    -- ============================================================
    plugin_1 = {
        moduletype = "RemotePass",
        exec_path = EXECUTABLE_PATH .. formatExecutable("remote_cpu_a76_mhu"),
        remote_argv = {"--param", "log_level=1"},
        tlm_initiator_ports_num = 1,
        tlm_target_ports_num = 0,
        target_signals_num = 2,      -- MHU_A76_rx(0), MHU_A76_tx(1)
        initiator_signals_num = 0,
        initiator_socket_0 = {bind = "&router.target_socket"},

        plugin_pass = {
            moduletype = "RemotePass",
            tlm_initiator_ports_num = 0,
            tlm_target_ports_num = 1,
            target_signals_num = 0,
            initiator_signals_num = 2,
            target_socket_0 = {
                address = 0,
                size = 0x100000000,
                priority = 100,
                bind = "&router.initiator_socket",
            },
            initiator_signal_socket_0 = {bind = "&gic_0.spi_in_78"},  -- MHU A76 mbx (INT_ID 110 = SPI[78])
            initiator_signal_socket_1 = {bind = "&gic_0.spi_in_46"},  -- MHU A76 pbx (INT_ID  78 = SPI[46])
        },

        qemu_inst_mgr = { moduletype = "QemuInstanceManager" },
        qemu_inst = {
            moduletype = "QemuInstance",
            args = {"&qemu_inst_mgr", "AARCH64"},
            sync_policy = "multithread-freerunning",
        },

        router = { moduletype = "router", log_level = 0 },

        global_peripheral_initiator_arm_0 = {
            moduletype = "global_peripheral_initiator",
            args = {"&qemu_inst", "&cpu_0"},
            global_initiator = {bind = "&router.target_socket"},
        },

        gic_0 = {
            moduletype = "arm_gicv3",
            args = {"&qemu_inst"},
            dylib_path = "arm_gicv3",
            dist_iface = {address = GICD_BASE_A76, size = GICD_SIZE_A76, bind = "&router.initiator_socket"},
            redist_iface_0 = {address = GICR_BASE_A76, size = GICR_SIZE_A76, bind = "&router.initiator_socket"},
            num_cpus = 1,
            num_spi = 96,
            redist_region = {1},
            irq_out_0 = {bind = "&cpu_0.irq_in"},
            fiq_out_0 = {bind = "&cpu_0.fiq_in"},
            virq_out_0 = {bind = "&cpu_0.virq_in"},
            vfiq_out_0 = {bind = "&cpu_0.vfiq_in"},
        },

        cpu_0 = {
            moduletype = "cpu_arm_cortexA76",
            args = {"&qemu_inst"},
            dylib_path = "cpu_arm_cortexA76",
            mem = {bind = "&router.target_socket"},
            rvbar = DDR_BASE + 0x100000,    -- A76 firmware in DDR
            has_el2 = false,
            has_el3 = false,
            psci_conduit = "disabled",
            mp_affinity = 0,
            start_powered_off = false,
            irq_timer_phys_out = {bind = "&gic_0.ppi_in_cpu_0_30"},
        },
    },
}
