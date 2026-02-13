library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity triangle_50hz_pwm is
    generic (
        CLK_FREQ : integer := 50000000 
    );
    port (
        clk      : in  std_logic;
        rst_n    : in  std_logic;
        pwm_out  : out std_logic
    );
end entity;

architecture rtl of triangle_50hz_pwm is

    -- Accumulatore di fase a 32 bit per precisione frequenza
    signal phase_acc : unsigned(31 downto 0) := (others => '0');
    constant PHASE_INC : unsigned(31 downto 0) := to_unsigned(
        integer((50.0 * 4294967296.0) / real(CLK_FREQ)), 32);
    
    -- Segnali per il valore triangolare e il PWM
    signal tri_val  : unsigned(9 downto 0);
    signal pwm_cnt  : unsigned(9 downto 0) := (others => '0');

begin

    -- Logica per generare l'onda triangolare a 10 bit (0-1023)
    -- Usiamo l'11° bit dell'accumulatore (bit 31) come indicatore di salita/discesa
    -- I bit dal 30 al 21 ci danno l'ampiezza a 10 bit
    process(phase_acc)
    begin
        if phase_acc(31) = '0' then
            -- Fase di salita: prendiamo i bit così come sono
            tri_val <= phase_acc(30 downto 21);
        else
            -- Fase di discesa: invertiamo i bit (complemento a 1)
            tri_val <= not phase_acc(30 downto 21);
        end if;
    end process;

    -- Processo principale per Phase Accumulator e PWM
    process(clk)
    begin
        if rising_edge(clk) then
            if rst_n = '0' then
                phase_acc <= (others => '0');
                pwm_cnt   <= (others => '0');
                pwm_out   <= '0';
            else
                -- 1. Incremento fase (determina i 50Hz)
                phase_acc <= phase_acc + PHASE_INC;

                -- 2. Incremento contatore PWM (determina la portante PWM)
                pwm_cnt <= pwm_cnt + 1;

                -- 3. Generazione uscita PWM
                if pwm_cnt < tri_val then
                    pwm_out <= '1';
                else
                    pwm_out <= '0';
                end if;
            end if;
        end if;
    end process;

end architecture;