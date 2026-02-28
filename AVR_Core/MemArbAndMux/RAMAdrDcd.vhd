--************************************************************************************************
--  Address decoder
--  Version 0.11A 
--  Designed by Ruslan Lepetenok 
--  Modified 31.07.2005
--************************************************************************************************

library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.std_logic_unsigned.all;

use WORK.MemAccessCtrlPack.all;

entity RAMAdrDcd is port(
                       ramadr    : in std_logic_vector(15 downto 0);
		                 ramre     : in std_logic;
		                 ramwe     : in std_logic;
		                 -- Memory mapped I/O i/f
		                 stb_IO	   : out std_logic;
		                 stb_IOmod : out std_logic_vector(CNumOfSlaves-1 downto 0);		
	                     -- Data memory i/f
		                 ram_we    : out std_logic;
		                 ram_ce    : out std_logic;
							  bram_ce   : out std_logic;
							  ram_sel   : out std_logic
		                );
end RAMAdrDcd;

architecture RTL of RAMAdrDcd is
    signal ram_sel_int  : std_logic;
    signal bram_sel_int : std_logic; -- Nuovo segnale interno
begin 

-- 1. Identifichiamo l'area BRAM (0x4000 - 0x7FFF)
bram_sel_int <= '1' when (ramadr(15 downto 14) = "01") else '0';
bram_ce      <= bram_sel_int;

-- 2. Identifichiamo l'area RAM standard (solitamente parte da 0x0100)
-- AGGIUNTA: la RAM standard risponde SOLO se non siamo nell'area BRAM
ram_sel_int <= '1' when (ramadr(ramadr'high downto ramadr'high-CDRAMBaseAdr'high) = CDRAMBaseAdr) 
               and (bram_sel_int = '0') else '0'; 

-- 3. Segnali di controllo per la RAM standard
ram_sel <= ram_sel_int; 
ram_we  <= ram_sel_int and ramwe;   
ram_ce  <= ram_sel_int and (ramwe or ramre);

-- I/O Mapped (Inalterato)
stb_IO       <= '1' when (ramadr(15 downto 8) = x"00") else '0'; -- Tipico per AVR I/O
stb_IOmod(0) <= '1' when ramadr(7 downto 4)=x"0" else '0';
stb_IOmod(1) <= '1' when ramadr(7 downto 4)=x"1" else '0';

end RTL;

