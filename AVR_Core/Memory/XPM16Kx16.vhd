-- **********************************************************************************************
-- 16Kx16 ROM wrapper for AVR Core (Intel/Altera IP Catalog)
-- Drop-in replacement per Xilinx XPM16Kx16
-- **********************************************************************************************
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity XPM16Kx16 is
    port(
        cp2     : in  std_logic;
        ce      : in  std_logic;
        address : in  std_logic_vector(13 downto 0);
        din     : in  std_logic_vector(15 downto 0);  -- ignorato (ROM)
        dout    : out std_logic_vector(15 downto 0);
        weh     : in  std_logic;  -- ignorati (ROM)
        wel     : in  std_logic
    );
end XPM16Kx16;

architecture RTL of XPM16Kx16 is

    component rom16kx16
        port(
            clock  : in  std_logic;
            address: in  std_logic_vector(13 downto 0);
            q      : out std_logic_vector(15 downto 0)
        );
    end component;

    signal q_int : std_logic_vector(15 downto 0);

begin

    -- istanza ROM generata da IP Catalog
    ROM_INST : rom16kx16
        port map(
            clock   => cp2,
            address => address,
            q       => q_int
        );

    -- Enable di uscita come nel modello Xilinx
    dout <= q_int when ce = '1' else (others => '0');

end RTL;
