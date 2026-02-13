library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity clk_divider_100Hz is
    port (
        clk_in    : in  std_logic;  -- Ingresso 50 MHz
        reset_n   : in  std_logic;  -- Reset asincrono NEGATO (Attivo basso)
        clk_out   : out std_logic   -- Uscita 100 Hz (onda quadra 50%)
    );
end entity;

architecture rtl of clk_divider_100Hz is
    -- Calcolo: (50.000.000 / 100) / 2 = 250.000
    constant DIVISOR : unsigned(17 downto 0) := to_unsigned(250000, 18);
    
    signal counter   : unsigned(17 downto 0) := (others => '0');
    signal clk_reg   : std_logic := '0';
begin

    process(clk_in, reset_n)
    begin
        if reset_n = '0' then         -- Qui la logica è negata
            counter <= (others => '0');
            clk_reg <= '0';
        elsif rising_edge(clk_in) then
            if counter = (DIVISOR - 1) then
                counter <= (others => '0');
                clk_reg <= not clk_reg; -- Toggle per il 50% duty cycle
            else
                counter <= counter + 1;
            end if;
        end if;
    end process;

    clk_out <= clk_reg;

end architecture;