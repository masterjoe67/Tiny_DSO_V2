library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity SmartEncoderBank is
    port (
        clk             : in  std_logic; -- 60 MHz
        rst_n           : in  std_logic;
        
        -- Ingressi fisici encoder
        enc_a           : in  std_logic_vector(6 downto 0);
        enc_b           : in  std_logic_vector(6 downto 0);
        
        -- Interfaccia Bus AVR
        addr            : in  std_logic_vector(2 downto 0); -- 0..7 (SEL, VAL, MIN, MAX, STEP...)
        data_in         : in  std_logic_vector(15 downto 0);
        data_out        : out std_logic_vector(15 downto 0);
        we              : in  std_logic  -- Write Enable dal bus
    );
end SmartEncoderBank;

architecture Behavioral of SmartEncoderBank is

    -- Struttura dati per ogni encoder
    type enc_reg_t is record
        val  : signed(15 downto 0);
        min  : signed(15 downto 0);
        max  : signed(15 downto 0);
        step : signed(15 downto 0);
    end record;

    type enc_bank_t is array (0 to 6) of enc_reg_t;
    signal bank : enc_bank_t := (others => (x"0000", x"8000", x"7FFF", x"0001"));

    -- Segnale per la selezione dell'encoder corrente (0-6)
    signal selected_enc : integer range 0 to 7 := 0;

    -- Filtro e rilevamento fronti
    signal pipe_a, pipe_b : std_logic_vector(6 downto 0) := (others => '0');
    signal last_a         : std_logic_vector(6 downto 0) := (others => '0');
    signal clean_a, clean_b : std_logic_vector(6 downto 0) := (others => '0');

begin

    -- 1. FILTRO DIGITALE E DEBOUNCE (semplificato per efficienza)
    process(clk)
    begin
        if rising_edge(clk) then
            -- Shift register per eliminare spike veloci
            pipe_a <= enc_a;
            pipe_b <= enc_b;
            -- Campionamento sincronizzato
            clean_a <= pipe_a;
            clean_b <= pipe_b;
        end if;
    end process;

    -- 2. LOGICA DI CONTROLLO ENCODER E BUS
    process(clk, rst_n)
        variable next_v : signed(15 downto 0);
    begin
        if rst_n = '0' then
            selected_enc <= 0;
            -- Valori di default (esempio)
            for i in 0 to 6 loop
                bank(i).val <= (others => '0');
                bank(i).min <= x"8001"; -- Minimo signed 16-bit
                bank(i).max <= x"7FFF"; -- Massimo signed 16-bit
                bank(i).step <= x"0001";
            end loop;
        elsif rising_edge(clk) then
            
            -- GESTIONE SCRITTURA BUS AVR
            if we = '1' then
                case addr is
                    when "000" => -- REG_ENC_SEL
                        if unsigned(data_in) < 7 then
                            selected_enc <= to_integer(unsigned(data_in(2 downto 0)));
                        end if;
                    when "001" => bank(selected_enc).val  <= signed(data_in);
                    when "010" => bank(selected_enc).min  <= signed(data_in);
                    when "011" => bank(selected_enc).max  <= signed(data_in);
                    when "100" => bank(selected_enc).step <= signed(data_in);
                    when others => null;
                end case;
            end if;

            -- GESTIONE MOVIMENTO FISICO (Controlla tutti i 7 encoder)
            for i in 0 to 6 loop
                if clean_a(i) = '1' and last_a(i) = '0' then -- Fronte salita Fase A
                    if clean_b(i) = '0' then -- Senso orario
                        next_v := bank(i).val + bank(i).step;
                    else -- Senso antiorario
                        next_v := bank(i).val - bank(i).step;
                    end if;

                    -- Applicazione Limiti (Saturazione)
                    if next_v > bank(i).max then
                        bank(i).val <= bank(i).max;
                    elsif next_v < bank(i).min then
                        bank(i).val <= bank(i).min;
                    else
                        bank(i).val <= next_v;
                    end if;
                end if;
            end loop;
            
            last_a <= clean_a;
        end if;
    end process;

    -- 3. GESTIONE LETTURA BUS AVR
    process(addr, bank, selected_enc)
    begin
        case addr is
            when "000" => data_out <= std_logic_vector(to_unsigned(selected_enc, 16));
            when "001" => data_out <= std_logic_vector(bank(selected_enc).val);
            when "010" => data_out <= std_logic_vector(bank(selected_enc).min);
            when "011" => data_out <= std_logic_vector(bank(selected_enc).max);
            when "100" => data_out <= std_logic_vector(bank(selected_enc).step);
            when others => data_out <= (others => '0');
        end case;
    end process;

end Behavioral;