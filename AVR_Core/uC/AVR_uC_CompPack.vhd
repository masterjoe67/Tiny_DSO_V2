--************************************************************************************************
-- Component declarations for AVR core
-- Version 2.6A 
-- Designed by Ruslan Lepetenok 
-- Modified 31.05.2006
--************************************************************************************************

library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;
use WORK.AVRuCPackage.all;

package AVR_uC_CompPack is

component pport is generic(PPortNum : natural); 
	               port(
	                   -- AVR Control
               ireset     : in std_logic;
               cp2	      : in std_logic;
               adr        : in std_logic_vector(6 downto 0);
               dbus_in    : in std_logic_vector(7 downto 0);
               dbus_out   : out std_logic_vector(7 downto 0);
               iore       : in std_logic;
               iowe       : in std_logic;
               out_en     : out std_logic; 
			            -- External connection
			   portx      : out std_logic_vector(7 downto 0);
			   ddrx       : out std_logic_vector(7 downto 0);
			   pinx       : in  std_logic_vector(7 downto 0));
end component;


component mmio_encoder is
  port (
    clk          : in  std_logic;
    reset        : in  std_logic;
	 set_origin	  : IN     STD_LOGIC;
    direction    : OUT    STD_LOGIC;                            --direction of last change, 1 = positive, 0 = negative

    -- encoder signals
    enc_a     : in  std_logic;
    enc_b     : in  std_logic;

    -- bus interface
    bus_addr  : in  std_logic_vector(6 downto 0);
    bus_wdata : in  std_logic_vector(7 downto 0);
    bus_rdata : out std_logic_vector(7 downto 0);
    bus_rd    : in  std_logic;
    bus_wr    : in  std_logic;

    out_en    : out std_logic
  );
end component;

component SmartEncoderBank is
    port (
        clk          : in  std_logic;
        rst_n        : in  std_logic;
        -- Hardware Encoder (14 pin)
        enc_a        : in  std_logic_vector(6 downto 0);
        enc_b        : in  std_logic_vector(6 downto 0);
        -- Bus Interfaccia
        addr_in      : in  std_logic_vector(6 downto 0); 
        data_in      : in  std_logic_vector(7 downto 0);
        data_out     : out std_logic_vector(7 downto 0);
        iowe         : in  std_logic;
        iore         : in  std_logic;
        out_en       : out std_logic  -- ABILITAZIONE BUS DATI
    );
end component;


component keypad_5x3 is
    port (
        clk             : in  std_logic;
        rst_n           : in  std_logic;

        -- GPIO
        rows_i          : in  std_logic_vector(4 downto 0); -- 5 INGRESSI matrice
        cols_o          : out std_logic_vector(2 downto 0); -- 3 USCITE matrice
        enc_keys_i      : in  std_logic_vector(3 downto 0); -- 4 TASTI DEGLI ENCODER

        -- MMIO
        mmio_we         : in  std_logic;
        mmio_re         : in  std_logic;
        mmio_addr       : in  std_logic_vector(6 downto 0);
        mmio_wdata      : in  std_logic_vector(7 downto 0);
        mmio_rdata      : out std_logic_vector(7 downto 0);
        out_en          : out std_logic
    );
end component;


component external_mux is port (
		  ramre              : in  std_logic;
		  dbus_out           : out std_logic_vector (7 downto 0);
		  ram_data_out       : in  std_logic_vector (7 downto 0);
		  io_port_bus        : in  ext_mux_din_type;
		  io_port_en_bus     : in  ext_mux_en_type;
          irqack             : in  std_logic;
          irqackad           : in  std_logic_vector(4 downto 0);		  
		  ind_irq_ack        : out std_logic_vector(22 downto 0)		  
		  );
end component;


component RAMDataReg is port(	                   
               ireset      : in  std_logic;
               cp2	       : in  std_logic;
               cpuwait     : in  std_logic;
			   RAMDataIn   : in  std_logic_vector(7 downto 0);
			   RAMDataOut  : out std_logic_vector(7 downto 0)
	                     );
end component;


component Timer_Counter is port(
	                         -- AVR Control
                             ireset         : in  std_logic;
                             cp2	        : in  std_logic;
							 cp2en          : in  std_logic;
							 tmr_cp2en      : in  std_logic;
							 stopped_mode   : in  std_logic; -- ??
						     tmr_running    : in  std_logic; -- ??
                             adr            : in  std_logic_vector(6 downto 0);
                             dbus_in        : in  std_logic_vector(7 downto 0);
                             dbus_out       : out std_logic_vector(7 downto 0);
                             iore           : in  std_logic;
                             iowe           : in  std_logic;
                             out_en         : out std_logic; 
                             -- External inputs/outputs
                             EXT1           : in  std_logic;
                             EXT2           : in  std_logic;
			                 OC0_PWM0       : out std_logic;
			                 OC1A_PWM1A     : out std_logic;
			                 OC1B_PWM1B     : out std_logic;
			                 OC2_PWM2       : out std_logic;
			                 -- Interrupt related signals
                             TC0OvfIRQ      : out std_logic;
			                 TC0OvfIRQ_Ack  : in  std_logic;
			                 TC0CmpIRQ      : out std_logic;
			                 TC0CmpIRQ_Ack  : in  std_logic;
			                 TC2OvfIRQ      : out std_logic;
			                 TC2OvfIRQ_Ack  : in  std_logic;
			                 TC2CmpIRQ      : out std_logic;
			                 TC2CmpIRQ_Ack  : in  std_logic;
			                 TC1OvfIRQ      : out std_logic;
			                 TC1OvfIRQ_Ack  : in  std_logic;
			                 TC1CmpAIRQ     : out std_logic;
			                 TC1CmpAIRQ_Ack : in  std_logic;
			                 TC1CmpBIRQ     : out std_logic;
			                 TC1CmpBIRQ_Ack : in  std_logic;			   
			                 TC1ICIRQ       : out std_logic;
			                 TC1ICIRQ_Ack   : in  std_logic
							 );
end component;


----*************** UART ***************************


-- Core itself
component AVR_Core is port(
                        --Clock and reset
	                    cp2         : in std_logic;
                        cp2en       : in std_logic;
						ireset      : in std_logic;
			            -- JTAG OCD support
					    valid_instr : out std_logic;
						insert_nop  : in  std_logic; 
						block_irq   : in  std_logic;
						change_flow : out std_logic;
                        -- Program Memory
                        pc          : out std_logic_vector (15 downto 0);   
                        inst        : in  std_logic_vector (15 downto 0);
                        -- I/O control
                        adr         : out std_logic_vector (6 downto 0); 	
                        iore        : out std_logic;                       
                        iowe        : out std_logic;						
                        -- Data memory control
                        ramadr      : out std_logic_vector (15 downto 0);
                        ramre       : out std_logic;
                        ramwe       : out std_logic;
						cpuwait     : in  std_logic;
						-- Data paths
                        dbusin      : in  std_logic_vector (7 downto 0);
                        dbusout     : out std_logic_vector (7 downto 0);
                        -- Interrupt
                        irqlines    : in  std_logic_vector (22 downto 0);
                        irqack      : out std_logic;
                        irqackad    : out std_logic_vector(4 downto 0);
                        --Sleep Control
                        sleepi	    : out std_logic;
                        irqok	    : out std_logic;
                        globint	    : out std_logic;
                        --Watchdog
                        wdri	    : out std_logic);
end component;


-- Reset generator
component ResetGenerator is port(
	                            -- Clock inputs
								cp2	       : in  std_logic;
								cp64m	   : in  std_logic;
								-- Reset inputs
	                            nrst       : in  std_logic;
								npwrrst    : in  std_logic;
								wdovf      : in  std_logic;
			                    jtagrst    : in  std_logic;
      							-- Reset outputs
					            nrst_cp2   : out std_logic;
			                    nrst_cp64m : out std_logic;
								nrst_clksw : out std_logic
								);
end component;

-- Components for the simulation only

component PROM is port(
                       address_in : in  std_logic_vector (15 downto 0);
                       data_out   : out std_logic_vector (15 downto 0));
end component;


component DataRAM is 
		  generic(RAMSize :positive);
	      port (
          cp2         : in std_logic;
  	      address     : in  std_logic_vector (LOG2(RAMSize)-1 downto 0);
		  ramwe		  : in  std_logic;
		  din         : in  std_logic_vector (7 downto 0);
		  dout        : out	std_logic_vector (7 downto 0));
end component;


component CPUWaitGenerator is port(
	           ireset  : in  std_logic;
			   cp2     : in  std_logic;
			   ramre   : in  std_logic;
			   ramwe   : in  std_logic;
			   cpuwait : out std_logic                 
			        );
end component;

component ClockSwitch is port(
						   -- Reset
	                       ireset   : in  std_logic;
						   -- Clock input and output
						   cp2_In   : in  std_logic;
						   cp2_Out  : out std_logic;
	                       -- Control inputs
						   sleepi   : in  std_logic;
                           irqok    : in  std_logic;
                           globint  : in  std_logic;
						   sleep_en	: in  std_logic
						   );
end component;


-- JTAG
component JTAGOCDPrgTop is port(
	                      -- AVR Control
                          ireset       : in  std_logic;
                          cp2	       : in  std_logic;
						  -- JTAG related inputs/outputs
						  TRSTn        : in  std_logic; -- Optional
	                      TMS          : in  std_logic;
                          TCK	       : in  std_logic;
                          TDI          : in  std_logic;
                          TDO          : out std_logic;
						  TDO_OE       : out std_logic;
						  -- From the core
                          PC           : in  std_logic_vector(15 downto 0);
						  -- To the PM("Flash")
						  pm_adr       : out std_logic_vector(15 downto 0);   
						  pm_h_we      : out std_logic;
						  pm_l_we      : out std_logic;
						  pm_dout      : in  std_logic_vector(15 downto 0);  
						  pm_din       : out std_logic_vector(15 downto 0);
						  -- To the "EEPROM" 
						  EEPrgSel     : out std_logic;
						  EEAdr        : out std_logic_vector(11 downto 0);    
						  EEWrData     : out std_logic_vector(7 downto 0);
						  EERdData     : in  std_logic_vector(7 downto 0);
						  EEWr         : out std_logic;
						  -- CPU reset
						  jtag_rst     : out std_logic
                          );
end component;


component uart is port(
	                -- AVR Control
                    ireset     : in  std_logic;
                    cp2	       : in  std_logic;
                    adr        : in  std_logic_vector(6 downto 0);
                    dbus_in    : in  std_logic_vector(7 downto 0);
                    dbus_out   : out std_logic_vector(7 downto 0);
                    iore       : in  std_logic;
                    iowe       : in  std_logic;
                    out_en     : out std_logic; 
                    -- UART
                    rxd        : in  std_logic;
                    rx_en      : out std_logic;
                    txd        : out std_logic;
                    tx_en      : out std_logic;
                    -- IRQ
                    txcirq     : out std_logic;
                    txc_irqack : in  std_logic;
                    udreirq    : out std_logic;
			        rxcirq     : out std_logic
		            );
end component;






component FrqDiv is port(
                      clk_in     : in  std_logic;
			          clk_out    : out std_logic
		              );
end component;


component adc128s022_reader is
    port (
        clk     : in  std_logic;
        rst_n   : in  std_logic;

        -- SPI ADC
        sclk    : out std_logic;
        cs_n    : out std_logic;
        mosi    : out std_logic;
        miso    : in  std_logic;

        -- Uscite dati
        ch0   : out unsigned(11 downto 0);
        ch1   : out unsigned(11 downto 0);
        ch2   : out unsigned(11 downto 0)

    );
end component;

component dp_ram_1024x12 is
    port (
        clk_wr   : in  std_logic;
        addr_wr  : in  unsigned(9 downto 0);
        data_in  : in  unsigned(11 downto 0);
        wr_en    : in  std_logic;

        clk_rd   : in  std_logic;
        addr_rd  : in  unsigned(9 downto 0);
        data_out : out unsigned(11 downto 0)
    );
end component;

component dp_ram_4096x12 is
    port (
        clk_wr   : in  std_logic;
        addr_wr  : in  unsigned(11 downto 0);
        data_in  : in  unsigned(11 downto 0);
        wr_en    : in  std_logic;

        clk_rd   : in  std_logic;
        addr_rd  : in  unsigned(11 downto 0);
        data_out : out unsigned(11 downto 0)
    );
end component;



component oscilloscope_top is
    port(
        clk     : in  std_logic;
        rst_n   : in  std_logic;

        -- ADC
        sclk    : out std_logic;
        cs_n    : out std_logic;
        miso    : in  std_logic;

        -- MMIO
        iore        : in  std_logic;
        mmio_addr   : in  std_logic_vector(7 downto 0);
        mmio_wdata  : in  std_logic_vector(7 downto 0);
        mmio_we     : in  std_logic;
        mmio_rdata  : out std_logic_vector(7 downto 0);
        out_en      : out std_logic
    );
end component;

component st7796_fast_ctrl is
    port (
        clk         : in  std_logic;    -- 40MHz
		  clk_spi     : in  std_logic;
        rst_n       : in  std_logic;
        -- Interfaccia Bus AVR
        adr         : in  std_logic_vector(IOAdrWidth-1 downto 0);
        dbus_in     : in  std_logic_vector(7 downto 0);
        dbus_out    : out std_logic_vector(7 downto 0);
        iore        : in  std_logic;
        iowe        : in  std_logic;
		  out_en      : out std_logic;    -- Attivazione driver bus dati
        -- Pin Fisici Display
        tft_sclk    : out std_logic;
        tft_mosi    : out std_logic;
        tft_cs      : out std_logic;
        tft_dc      : out std_logic;
        tft_rst     : out std_logic;
		  tft_backlight : out std_logic
    );
end component;



end AVR_uC_CompPack;
