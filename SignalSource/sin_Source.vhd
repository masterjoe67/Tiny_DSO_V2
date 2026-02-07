--library ieee;
--use ieee.std_logic_1164.all;
--use ieee.numeric_std.all;
--
--entity sine_50hz_hex is
--    generic (
--        CLK_FREQ : integer := 50000000 -- Il tuo clock (es. 50MHz)
--    );
--    port (
--        clk     : in  std_logic;
--        rst_n   : in  std_logic;
--        sine_o  : out std_logic_vector(7 downto 0)
--    );
--end entity;
--
--architecture rtl of sine_50hz_hex is
--
--    -- Definizione della ROM da 2048 campioni x 8 bit
--    type rom_type is array (0 to 2047) of std_logic_vector(7 downto 0);
--    
--    -- Attributo per dire a Quartus di caricare il file .hex
--    signal sine_rom : rom_type;
--    attribute ram_init_file : string;
--    attribute ram_init_file of sine_rom : signal is "sin_lut_2048.hex";
--
--    -- Accumulatore di fase a 32 bit
--    signal phase_acc : unsigned(31 downto 0) := (others => '0');
--    
--    -- Calcolo incremento: (Fout * 2^32) / Fclk
--    -- Per 50Hz su 50MHz il valore rimane ~4295
--    constant PHASE_INC : unsigned(31 downto 0) := to_unsigned(
--        integer((50.0 * 4294967296.0) / real(CLK_FREQ)), 32);
--
--begin
--
--    process(clk)
--    begin
--        if rising_edge(clk) then
--            if rst_n = '0' then
--                phase_acc <= (others => '0');
--            else
--                phase_acc <= phase_acc + PHASE_INC;
--            end if;
--        end if;
--    end process;
--
--    -- Usiamo gli 11 bit più significativi (31 downto 21) 
--    -- per indirizzare i 2048 campioni (2^11 = 2048)
--    sine_o <= sine_rom(to_integer(phase_acc(31 downto 21)));
--
--end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.math_real.all; -- Usiamo la generazione matematica per evitare errori di file

entity sine_50hz_hex is
    generic (
        CLK_FREQ : integer := 50000000 
    );
    port (
        clk      : in  std_logic;
        rst_n    : in  std_logic;
        pwm_out  : out std_logic
    );
end entity;

architecture rtl of sine_50hz_hex is

    -- 1. ROM: 2048 campioni, ognuno da 10 bit
    type rom_type is array (0 to 2047) of std_logic_vector(9 downto 0);
    
    function init_rom return rom_type is
        variable temp_rom : rom_type;
        variable sn_real  : real;
    begin
        for i in 0 to 2047 loop
            sn_real := sin(2.0 * MATH_PI * real(i) / 2048.0);
            -- Scaliamo su 10 bit: range 0 - 1023 (offset 512)
            temp_rom(i) := std_logic_vector(to_unsigned(integer(511.0 * sn_real + 512.0), 10));
        end loop;
        return temp_rom;
    end function;

    constant SINE_ROM : rom_type := init_rom;

    -- 2. Accumulatore di fase (per i 50Hz)
    signal phase_acc : unsigned(31 downto 0) := (others => '0');
    constant PHASE_INC : unsigned(31 downto 0) := to_unsigned(
        integer((300.0 * 4294967296.0) / real(CLK_FREQ)), 32);
    
    -- 3. PWM a 10 bit
    signal pwm_cnt : unsigned(9 downto 0) := (others => '0');
    signal current_val : std_logic_vector(9 downto 0);

begin

    -- Lettura della ROM (11 bit di indirizzo: 2^11 = 2048)
    current_val <= SINE_ROM(to_integer(phase_acc(31 downto 21)));

    process(clk)
    begin
        if rising_edge(clk) then
            if rst_n = '0' then
                phase_acc <= (others => '0');
                pwm_cnt   <= (others => '0');
                pwm_out   <= '0';
            else
                -- Avanzamento fase sinusoide
                phase_acc <= phase_acc + PHASE_INC;

                -- Contatore PWM (ora conta fino a 1023)
                pwm_cnt <= pwm_cnt + 1;

                -- Confronto a 10 bit
                if pwm_cnt < unsigned(current_val) then
                    pwm_out <= '1';
                else
                    pwm_out <= '0';
                end if;
            end if;
        end if;
    end process;

end architecture;