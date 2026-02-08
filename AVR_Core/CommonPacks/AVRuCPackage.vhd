-- *****************************************************************************************
-- AVR constants and type declarations
-- Version 1.0A(Special version for the JTAG OCD)
-- Modified 05.05.2004
-- Designed by Ruslan Lepetenok
-- *****************************************************************************************

library	IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

use WORK.SynthCtrlPack.all;

package AVRuCPackage is
-- Old package
type ext_mux_din_type is array(0 to CExtMuxInSize-1) of std_logic_vector(7 downto 0);
subtype ext_mux_en_type  is std_logic_vector(0 to CExtMuxInSize-1);
-- End of old package

constant IOAdrWidth : positive := 7;

type AVRIOAdr_Type is array(0 to 127) of std_logic_vector(IOAdrWidth-1 downto 0);

constant CAVRIOAdr : AVRIOAdr_Type := (
    "0000000","0000001","0000010","0000011",
    "0000100","0000101","0000110","0000111",
    "0001000","0001001","0001010","0001011",
    "0001100","0001101","0001110","0001111",
    "0010000","0010001","0010010","0010011",
    "0010100","0010101","0010110","0010111",
    "0011000","0011001","0011010","0011011",
    "0011100","0011101","0011110","0011111",
    "0100000","0100001","0100010","0100011",
    "0100100","0100101","0100110","0100111",
    "0101000","0101001","0101010","0101011",
    "0101100","0101101","0101110","0101111",
    "0110000","0110001","0110010","0110011",
    "0110100","0110101","0110110","0110111",
    "0111000","0111001","0111010","0111011",
    "0111100","0111101","0111110","0111111",
    "1000000","1000001","1000010","1000011",
    "1000100","1000101","1000110","1000111",
    "1001000","1001001","1001010","1001011",
    "1001100","1001101","1001110","1001111",
    "1010000","1010001","1010010","1010011",
    "1010100","1010101","1010110","1010111",
    "1011000","1011001","1011010","1011011",
    "1011100","1011101","1011110","1011111",
    "1100000","1100001","1100010","1100011",
    "1100100","1100101","1100110","1100111",
    "1101000","1101001","1101010","1101011",
    "1101100","1101101","1101110","1101111",
    "1110000","1110001","1110010","1110011",
    "1110100","1110101","1110110","1110111",
    "1111000","1111001","1111010","1111011",
    "1111100","1111101","1111110","1111111"
);

	
-- I/O port addresses

-- I/O register file
constant RAMPZ_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#3B#);
constant SPL_Address   : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#3D#);
constant SPH_Address   : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#3E#);
constant SREG_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#3F#);
-- End of I/O register file

-- UART
constant UDR_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#0C#);
constant UBRR_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#09#);
constant USR_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#0B#);
constant UCR_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#0A#);
-- End of UART	

-- Timer/Counter
constant TCCR0_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#33#);
constant TCCR1A_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#2F#);
constant TCCR1B_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#2E#);
constant TCCR2_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#25#);
constant ASSR_Address   : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#30#);
constant TIMSK_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#37#);
constant TIFR_Address   : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#36#);
constant TCNT0_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#32#);
constant TCNT2_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#24#);
constant OCR0_Address   : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#31#);
constant OCR2_Address   : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#23#);
constant TCNT1H_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#2D#);
constant TCNT1L_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#2C#);
constant OCR1AH_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#2B#);
constant OCR1AL_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#2A#);
constant OCR1BH_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#29#);
constant OCR1BL_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#28#);
constant ICR1AH_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#27#);
constant ICR1AL_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#26#);
-- End of Timer/Counter	

-- Service module
constant MCUCR_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#35#);
constant EIMSK_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#39#);
constant EIFR_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#38#);
constant EICR_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#3A#);
constant MCUSR_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#34#);
constant XDIV_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#3C#);
-- End of service module

-- FAST SPI 
constant LCD_DATA_REG  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#0D#); -- Ex SPDR
constant LCD_STATUS_REG: std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#0E#); -- Ex SPSR
constant LCD_CTRL_REG  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#0F#); -- Ex SPCR
-- End of SPI

-- PORTA addresses 
constant PORTA_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#1B#);
constant DDRA_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#1A#);
constant PINA_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#19#);

-- PORTB addresses 
constant PORTB_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#18#);
constant DDRB_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#17#);
constant PINB_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#16#);

-- PORTC addresses 
constant PORTC_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#15#);
constant DDRC_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#14#);
constant PINC_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#13#);

-- PWM_Reg
--constant BUF_0  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#00#);
--constant BUF_1  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#01#);
--constant BUF_2  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#02#);
--constant BUF_3  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#03#);
constant COMMIT  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#04#);
constant RSEL  	  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#05#);
constant CTRL       : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#06#);
constant MODE       : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#07#);

-- Debounce_Reg
--constant OFF_DB  		: std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#10#);
--constant OFF_EVT 		: std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#11#);
--constant OFF_MASK 	: std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#12#);
--constant OFF_CLEAR  	: std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#08#);

constant KEY_STATUS  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#10#);
constant KEY_CODE 	: std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#11#);
constant KEY_CTRL 	: std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#12#);


-- Encoder 
constant ENC_VAL_H     : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#1D#);
constant ENC_VAL_L     : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#1C#);
constant ENC_BASE 	  : std_logic_vector(3 downto 0) := "0111";

--Scope 
constant REG_CHA       : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#13#);
constant REG_CHB       : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#14#);
constant REG_TG_CTRL      : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#15#);
constant REG_TRIG   	  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#35#);
constant REG_INDEX     : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#3C#);
constant REG_FREQ0  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#00#);
constant REG_FREQ1  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#01#);
constant REG_FREQ2  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#02#);
constant REG_FREQ3  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#03#);
-- ******************** Parallel port address table **************************************
constant CMaxNumOfPPort : positive := 3;

type PPortAdrTbl_Type is record Port_Adr : std_logic_vector(IOAdrWidth-1 downto 0);
	                            DDR_Adr  : std_logic_vector(IOAdrWidth-1 downto 0);
	                            Pin_Adr  : std_logic_vector(IOAdrWidth-1 downto 0);
end record;

type PPortAdrTblArray_Type is array (0 to CMaxNumOfPPort-1) of PPortAdrTbl_Type;

constant PPortAdrArray : PPortAdrTblArray_Type := ((PORTA_Address,DDRA_Address,PINA_Address),  -- PORTA
                                                   (PORTB_Address,DDRB_Address,PINB_Address),  -- PORTB
																	(PORTC_Address,DDRC_Address,PINC_Address)); -- PORTC

-- ***************************************************************************************


	
-- Analog to digital converter
--constant ADCL_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#04#);
--constant ADCH_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#05#);
--constant ADCSR_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#06#);
--constant ADMUX_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#07#);

-- Analog comparator
--constant ACSR_Address  : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#08#);

-- Watchdog
constant WDTCR_Address : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#21#);

-- JTAG OCDR (ATmega128)
constant OCDR_Address   : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#22#);

-- JTAG OCDR (ATmega16)
--constant OCDR_Address   : std_logic_vector(IOAdrWidth-1 downto 0) := CAVRIOAdr(16#31#);

-- ***************************************************************************************

-- Function declaration
function LOG2(Number : positive) return natural;

end AVRuCPackage;

package	body AVRuCPackage is

-- Functions	
function LOG2(Number : positive) return natural is
variable Temp : positive;
begin
Temp := 1;
if Number=1 then 
 return 0;
  else 
   for i in 1 to integer'high loop
    Temp := 2*Temp; 
     if Temp>=Number then 
      return i;
     end if;
end loop;
end if;	
end LOG2;	
-- End of functions	

end AVRuCPackage;	
