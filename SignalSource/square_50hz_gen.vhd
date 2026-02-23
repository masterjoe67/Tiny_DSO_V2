library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity square_50hz_gen is
    generic (
       
        CLK_FREQ : integer := 50000000 
    );
    port (
        clk      : in  std_logic;
        rst_n    : in  std_logic;
        sq_out   : out std_logic
    );
end entity;

architecture rtl of square_50hz_gen is

    -- Accumulatore di fase a 32 bit
    signal phase_acc : unsigned(31 downto 0) := (others => '0');
    
    -- Calcolo dell'incremento per 50Hz esatti
    -- Formula: (Freq_out * 2^32) / Freq_clk
    constant PHASE_INC : unsigned(31 downto 0) := to_unsigned(
        integer((50.0 * 4294967296.0) / real(CLK_FREQ)), 32);

begin

    process(clk)
    begin
        if rising_edge(clk) then
            if rst_n = '0' then
                phase_acc <= (others => '0');
                sq_out    <= '0';
            else
                -- Incremento costante della fase
                phase_acc <= phase_acc + PHASE_INC;

                -- ONDA QUADRA: 
                -- Il bit 31 (MSB) sta a '0' per metà del ciclo (0 a 180°)
                -- e a '1' per l'altra metà (180° a 360°).
                -- Duty cycle perfetto al 50%.
                sq_out <= phase_acc(31);
            end if;
        end if;
    end process;

end architecture;