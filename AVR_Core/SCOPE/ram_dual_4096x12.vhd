library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity dp_ram_4096x12 is
    port (
        clk_wr   : in  std_logic;
        addr_wr  : in  unsigned(11 downto 0);
        data_in  : in  unsigned(11 downto 0);
        wr_en    : in  std_logic;

        clk_rd   : in  std_logic;
        addr_rd  : in  unsigned(11 downto 0);
        data_out : out unsigned(11 downto 0)
    );
end entity;

architecture rtl of dp_ram_4096x12 is

    type ram_type is array (0 to 4095) of unsigned(11 downto 0);
    signal mem : ram_type := (others => (others => '0'));
	 
	 attribute ramstyle : string;
    attribute ramstyle of mem : signal is "M10K";

    attribute preserve : boolean;
    attribute preserve of mem : signal is true;

begin

    -- WRITE PORT
    process(clk_wr)
    begin
        if rising_edge(clk_wr) then
            if wr_en='1' then
                mem(to_integer(addr_wr)) <= data_in;
            end if;
        end if;
    end process;

    -- READ PORT
    process(clk_rd)
    begin
        if rising_edge(clk_rd) then
            data_out <= mem(to_integer(addr_rd));
        end if;
    end process;

end architecture;

