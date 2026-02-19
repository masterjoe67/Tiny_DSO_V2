

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
		tft_sclk    	: out std_logic;
		tft_mosi    	: out std_logic;
		tft_cs      	: out std_logic;
		tft_dc      	: out std_logic;
		tft_rst     	: out std_logic;
		tft_backlight	: out std_logic;
		
			-- SMART ENCODER
		ENC_TBASE_A 	 :  IN  STD_LOGIC;
		ENC_TBASE_B 	 :  IN  STD_LOGIC;
		ENC_CH_1_POS_A :  IN  STD_LOGIC;
		ENC_CH_1_POS_B :  IN  STD_LOGIC;
		ENC_CH_1_POS_K :  IN  STD_LOGIC;
		ENC_CH_1_GAIN_A :  IN  STD_LOGIC;
		ENC_CH_1_GAIN_B :  IN  STD_LOGIC;
		ENC_CH_2_POS_A :  IN  STD_LOGIC;
		ENC_CH_2_POS_B :  IN  STD_LOGIC;
		ENC_CH_2_POS_K :  IN  STD_LOGIC;
		ENC_CH_2_GAIN_A :  IN  STD_LOGIC;
		ENC_CH_2_GAIN_B :  IN  STD_LOGIC;		
		ENC_TRIG_POS_A :  IN  STD_LOGIC;
		ENC_TRIG_POS_B :  IN  STD_LOGIC;
		ENC_TRIG_POS_K :  IN  STD_LOGIC;
		ENC_PAN_POS_A :  IN  STD_LOGIC;
		ENC_PAN_POS_B :  IN  STD_LOGIC;
		ENC_PAN_POS_K :  IN  STD_LOGIC;
		
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

COMPONENT top_avr_core_v8 PORT(
	nrst   : in    std_logic;
	clk    : in    std_logic;
	ck50   : in    std_logic;
	clk_spi : IN STD_LOGIC;
	-- Port 
	porta  : inout std_logic_vector(7 downto 0);
	portb  : inout std_logic_vector(7 downto 0);
	--portc  : inout std_logic_vector(7 downto 0);
	-- UART 
	rxd    : in    std_logic;
	txd    : out   std_logic;
	-- TFT SPI
	tft_sclk    : out std_logic;
	tft_mosi    : out std_logic;
	tft_cs      : out std_logic;
	tft_dc      : out std_logic;
	tft_rst     : out std_logic;
	tft_backlight : out std_logic;
	-- External interrupts
	INTx   : in    std_logic_vector(7 downto 0); 
	INT0	 : in    std_logic;

	-- JTAG related signals
	TMS    : in    std_logic;
	TCK	 : in    std_logic;
	TDI    : in    std_logic;
	TDO    : out   std_logic;
	TRSTn  : in    std_logic; -- Optional JTAG input
	
   --ADC SPI
   ADC_sclk	    	: out   std_logic;
	ADC_cs_n			: out   std_logic;
	ADC_miso		   : in    std_logic;
	ADC_mosi  		: out std_logic;
	
	--keys		: in    std_logic_vector(7 downto 0);
	key_rows 		: in  std_logic_vector(4 downto 0); -- 5 INGRESSI (pull-up)
	key_cols 		: out std_logic_vector(2 downto 0); -- 3 USCITE
	
	--Encoder
	s_enc_a        : in  std_logic_vector(6 downto 0);
   s_enc_b        : in  std_logic_vector(6 downto 0);
	enc_keys_i     : in  std_logic_vector(3 downto 0)
	
	);
END COMPONENT;

COMPONENT clk_divider_100Hz 
    port (
        clk_in    : in  std_logic;  -- Ingresso 50 MHz
        reset_n   : in  std_logic;  -- Reset asincrono NEGATO (Attivo basso)
        clk_out   : out std_logic   -- Uscita 100 Hz (onda quadra 50%)
    );
end COMPONENT;

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
SIGNAL	s_enc_a : std_logic_vector(6 downto 0);
SIGNAL	s_enc_b : std_logic_vector(6 downto 0);
SIGNAL	enc_keys_i : std_logic_vector(3 downto 0);

BEGIN 




b2v_inst : top_avr_core_v8
PORT MAP(nrst => nrst,
		 clk => clk0,
		 ck50 => clk50,
		 clk_spi => clk_spi,
		 rxd => rxd,
		 key_rows => KEY_ROWS,
		 key_cols => KEY_COLS,
		 enc_keys_i => enc_keys_i,
		 
		 porta => porta,
		 portb => portb,
		 txd => TX,
		 
		 	-- TFT SPI
		 tft_sclk    => tft_sclk,
		 tft_mosi    => tft_mosi,
		 tft_cs      => tft_cs,
		 tft_dc      => tft_dc,
		 tft_rst     => tft_rst,
		 tft_backlight => tft_backlight,
		 
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
		 
		 s_enc_a => s_enc_a,
		 s_enc_b => s_enc_b
);


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



s_enc_a(0) <= ENC_CH_1_POS_A;
s_enc_b(0) <= ENC_CH_1_POS_B;
enc_keys_i(0) <= '1'; -- <= ENC_CH_1_POS_K;

s_enc_a(1) <= ENC_CH_1_GAIN_A;
s_enc_b(1) <= ENC_CH_1_GAIN_B;

s_enc_a(2) <= ENC_CH_2_POS_A;
s_enc_b(2) <= ENC_CH_2_POS_B;
enc_keys_i(1) <= '1'; -- <= ENC_CH_2_POS_K;

s_enc_a(3) <= ENC_CH_2_GAIN_A;
s_enc_b(3) <= ENC_CH_2_GAIN_B;

s_enc_a(4) <= ENC_TBASE_A;
s_enc_b(4) <= ENC_TBASE_B;

s_enc_a(5) <= ENC_TRIG_POS_A;
s_enc_b(5) <= ENC_TRIG_POS_B;
enc_keys_i(2) <= '1'; -- <= ENC_TRIG_POS_K;

s_enc_a(6) <= ENC_PAN_POS_A;
s_enc_b(6) <= ENC_PAN_POS_B;
enc_keys_i(3) <= ENC_PAN_POS_K;




nrst <= KEY(0);
rxd <= RX;
LED <= porta;

END rtl;

