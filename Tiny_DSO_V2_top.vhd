

LIBRARY ieee;
USE ieee.std_logic_1164.all; 

LIBRARY work;

ENTITY Tiny_DSO_V2_top IS 
	PORT
	(
		CLOCK_50 :  IN  STD_LOGIC;
		RX :  IN  STD_LOGIC;
		ENC_A :  IN  STD_LOGIC;
		ENC_B :  IN  STD_LOGIC;

		
		KEY :  IN  STD_LOGIC_VECTOR(0 TO 0);
		KEY_ROWS :  IN  STD_LOGIC_VECTOR(4 DOWNTO 0);
		LED :  INOUT  STD_LOGIC_VECTOR(7 DOWNTO 0);
		porta :  INOUT  STD_LOGIC_VECTOR(7 DOWNTO 0);
		portb :  INOUT  STD_LOGIC_VECTOR(7 DOWNTO 0);
		TX :  OUT  STD_LOGIC;
			-- TFT SPI
		tft_sclk    : out std_logic;
		tft_mosi    : out std_logic;
		tft_cs      : out std_logic;
		tft_dc      : out std_logic;
		tft_rst     : out std_logic;
		
		ADC_miso :  IN  STD_LOGIC;
		ADC_sclk :  OUT  STD_LOGIC;
		ADC_cs_n :  OUT  STD_LOGIC;
		ADC_mosi :  OUT  STD_LOGIC;
		out100Hz :  OUT  STD_LOGIC;
		KEY_COLS :  OUT  STD_LOGIC_VECTOR(2 DOWNTO 0);
		PWM_A_H  :  OUT  STD_LOGIC;
		PWM_B_H  :  OUT  STD_LOGIC
		
	);
END Tiny_DSO_V2_top;

ARCHITECTURE rtl OF Tiny_DSO_V2_top IS 

COMPONENT top_avr_core_v8
	PORT(nrst : IN STD_LOGIC;
		 clk : IN STD_LOGIC;
		 ck50 : IN STD_LOGIC;
		 clk_spi : IN STD_LOGIC;
		 rxd : IN STD_LOGIC;
		 INT0 : IN STD_LOGIC;
		 TMS : IN STD_LOGIC;
		 TCK : IN STD_LOGIC;
		 TDI : IN STD_LOGIC;
		 TRSTn : IN STD_LOGIC;
		 ADC_miso : IN STD_LOGIC;
		 enc_a : IN STD_LOGIC;
		 enc_b : IN STD_LOGIC;
		 INTx : IN STD_LOGIC_VECTOR(7 DOWNTO 0);
		 key_rows : IN STD_LOGIC_VECTOR(4 DOWNTO 0);
		 porta : INOUT STD_LOGIC_VECTOR(7 DOWNTO 0);
		 portb : INOUT STD_LOGIC_VECTOR(7 DOWNTO 0);
		 txd : OUT STD_LOGIC;
		 TDO : OUT STD_LOGIC;
		 ADC_sclk : OUT STD_LOGIC;
		 ADC_cs_n : OUT STD_LOGIC;
		 ADC_mosi : OUT STD_LOGIC;
		 tft_sclk    : out std_logic;
		 tft_mosi    : out std_logic;
		 tft_cs      : out std_logic;
		 tft_dc      : out std_logic;
		 tft_rst     : out std_logic;

		 key_cols : OUT STD_LOGIC_VECTOR(2 DOWNTO 0)
	);
END COMPONENT;

COMPONENT sine_50hz_hex 
    generic (
        CLK_FREQ : integer := 50000000 -- Il tuo clock (es. 50MHz)
    );
    port (
        clk     : in  std_logic;
        rst_n   : in  std_logic;
        pwm_out  : out std_logic
    );
end COMPONENT;

COMPONENT triangle_50hz_pwm 
    generic (
        CLK_FREQ : integer := 50000000 
    );
    port (
        clk      : in  std_logic;
        rst_n    : in  std_logic;
        pwm_out  : out std_logic
    );
end COMPONENT;

COMPONENT pll_master
	PORT(inclk0 : IN STD_LOGIC;
		 c0 : OUT STD_LOGIC;
		 c1 : OUT STD_LOGIC;
		 c2 : OUT STD_LOGIC
	);
END COMPONENT;

COMPONENT count100hz
	PORT(clock : IN STD_LOGIC;
		 cout : OUT STD_LOGIC;
		 q : OUT STD_LOGIC_VECTOR(18 DOWNTO 0)
	);
END COMPONENT;

SIGNAL	clk0 :  STD_LOGIC;
SIGNAL	clk50 :  STD_LOGIC;
SIGNAL	clk_spi :  STD_LOGIC;
SIGNAL	nrst :  STD_LOGIC;
SIGNAL	rxd :  STD_LOGIC;



BEGIN 




b2v_inst : top_avr_core_v8
PORT MAP(nrst => nrst,
		 clk => clk0,
		 ck50 => clk50,
		 clk_spi => clk_spi,
		 rxd => rxd,

		 
		 enc_a => ENC_A,
		 enc_b => ENC_B,
		 key_rows => KEY_ROWS,
		 porta => porta,
		 portb => portb,
		 txd => TX,
		 
		 	-- TFT SPI
		 tft_sclk    => tft_sclk,
		 tft_mosi    => tft_mosi,
		 tft_cs      => tft_cs,
		 tft_dc      => tft_dc,
		 tft_rst     => tft_rst,

		 
		 ADC_miso => ADC_miso,
		 ADC_sclk => ADC_sclk,
		 ADC_cs_n => ADC_cs_n,
		 ADC_mosi => ADC_mosi,
		 -- JTAG related signals
		 TMS    => '0',
		 TCK	  => '0',
		 TDI     => '0',
		 TRSTn  => '0',
		 INTx   => (others => '0'),
		 INT0	  => '0',
		 key_cols => KEY_COLS);


b2v_inst1 : pll_master
PORT MAP(inclk0 => CLOCK_50,
		 c0 => clk0,
		 c1 => clk50,
		 c2 => clk_spi);


b2v_inst2 : count100hz
PORT MAP(clock => clk50,
		 cout => out100Hz);
		 
b2v_inst3 : sine_50hz_hex 
    generic map(
        CLK_FREQ => 50000000
    )
    port map(
        clk     => clk50,
        rst_n   => nrst,
        pwm_out  => PWM_A_H
    );
	 
b2v_inst4 : triangle_50hz_pwm 
    generic map(
        CLK_FREQ => 50000000
    )
    port map(
        clk     => clk50,
        rst_n   => nrst,
        pwm_out  => PWM_B_H
    );




nrst <= KEY(0);
rxd <= RX;
LED <= porta;

END rtl;

