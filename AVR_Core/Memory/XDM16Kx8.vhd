-- **********************************************************************************************
-- 16Kx8 Data Memory (DM) for AVR Core - Intel/Altera Version
-- Drop-in replacement dell’originale Xilinx XDM16Kx8
-- **********************************************************************************************

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity XDM16Kx8 is
    port(
        cp2     : in  std_logic;
        ce      : in  std_logic;
        address : in  std_logic_vector(13 downto 0);
        din     : in  std_logic_vector(7 downto 0);
        dout    : out std_logic_vector(7 downto 0);
        we      : in  std_logic
    );
end XDM16Kx8;

architecture RTL of XDM16Kx8 is

    component dm16kx8
        port(
            clock    : in  std_logic;
            address  : in  std_logic_vector(13 downto 0);
            data     : in  std_logic_vector(7 downto 0);
            wren     : in  std_logic;
            q        : out std_logic_vector(7 downto 0)
        );
    end component;

    signal q_int : std_logic_vector(7 downto 0);

begin

    DM_INST : dm16kx8
        port map(
            clock   => cp2,
            address => address,
            data    => din,
            wren    => we and ce,   -- abilitiamo scrittura solo se ce=1
            q       => q_int
        );

    -- output abilitato da ce come nel modello Xilinx
    dout <= q_int when ce = '1' else (others => '0');

end RTL;
