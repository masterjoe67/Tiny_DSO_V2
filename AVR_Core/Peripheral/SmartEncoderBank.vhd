library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.AVRuCPackage.all; -- Assicurati che questo package definisca ENC_INDEX, ENC_DATA_HI, ENC_DATA_LO

entity SmartEncoderBank is
    port (
        clk          : in  std_logic;
        rst_n        : in  std_logic;
        -- Hardware Encoder (14 pin)
        enc_a        : in  std_logic_vector(6 downto 0);
        enc_b        : in  std_logic_vector(6 downto 0);
        -- Bus Interfaccia
        addr_in      : in  std_logic_vector(6 downto 0); 
        data_in      : in  std_logic_vector(7 downto 0);
        data_out     : out std_logic_vector(7 downto 0);
        iowe         : in  std_logic;
        iore         : in  std_logic;
        out_en       : out std_logic  -- ABILITAZIONE BUS DATI
    );
end SmartEncoderBank;

architecture Behavioral of SmartEncoderBank is

    type enc_reg_t is record
        c_val  : signed(15 downto 0);
        min_v  : signed(15 downto 0);
        max_v  : signed(15 downto 0);
        step_v : signed(15 downto 0);
    end record;

    type enc_bank_t is array (0 to 6) of enc_reg_t;
    signal bank : enc_bank_t := (others => (x"0000", x"8001", x"7FFF", x"0001"));

    signal config_state : integer range 0 to 2 := 0;
    signal sel_enc      : integer range 0 to 6 := 0;
    signal sel_param    : std_logic_vector(1 downto 0) := "00";
    signal tmp_data_hi  : std_logic_vector(7 downto 0) := (others => '0');
    
    -- Segnali per Debounce
    signal last_a         : std_logic_vector(6 downto 0) := (others => '0');
    signal sampler_timer : unsigned(18 downto 0) := (others => '0'); 
    signal pipe_a, pipe_b : std_logic_vector(6 downto 0) := (others => '0');
    signal deb_a, deb_b   : std_logic_vector(6 downto 0) := (others => '0');

begin

    -- Generazione OUT_EN: attivo solo durante lettura indirizzi validi
    out_en <= '1' when (iore = '1' and (addr_in = ENC_INDEX or addr_in = ENC_DATA_HI or addr_in = ENC_DATA_LO)) else '0';

    -- 1. PROCESSO DI DEBOUNCE (Filtra i rimbalzi meccanici)
    process(clk)
    begin
        if rising_edge(clk) then
            -- Pipeline per sincronizzare i segnali asincroni
            pipe_a <= enc_a;
            pipe_b <= enc_b;

            -- Timer: conta sempre. Quando arriva a 0 (overflow), campiona.
            -- A 60MHz, 16 bit sono circa 1ms di ritardo. Perfetto per meccanici.
            sampler_timer <= sampler_timer + 1;
            
            if sampler_timer = 0 then
                deb_a <= pipe_a;
                deb_b <= pipe_b;
            end if;
        end if;
    end process;

    -- 2. LOGICA PRINCIPALE (Scrittura + Gestione Encoder)
    process(clk, rst_n)
        variable next_v : signed(15 downto 0);
        variable temp_vector : std_logic_vector(15 downto 0);
    begin
        if rst_n = '0' then
            config_state <= 0;
            bank <= (others => (x"0000", x"8001", x"7FFF", x"0001"));
            sel_enc <= 0;
        elsif rising_edge(clk) then
            
            -- A. RESET MACCHINA A STATI SE SI LEGGE
            -- Evita che l'FPGA resti bloccata se l'AVR fa solo una lettura
            if iore = '1' then
                config_state <= 0;
            end if;

            -- B. LOGICA DI SCRITTURA (Configurazione) - QUESTA MANCAVA!
            if iowe = '1' and addr_in = ENC_INDEX then
                case config_state is
                    when 0 => 
                        sel_enc   <= to_integer(unsigned(data_in(7 downto 4)));
                        sel_param <= data_in(3 downto 2);
                        config_state <= 1;
                    when 1 => 
                        tmp_data_hi <= data_in;
                        config_state <= 2;
                    when 2 => 
                        temp_vector := tmp_data_hi & data_in;
                        next_v      := signed(temp_vector);
                        case sel_param is
                            when "00" => bank(sel_enc).c_val  <= next_v;
                            when "01" => bank(sel_enc).min_v  <= next_v;
                            when "10" => bank(sel_enc).max_v  <= next_v;
                            when "11" => bank(sel_enc).step_v <= next_v;
                            when others => null;
                        end case;
                        config_state <= 0;
                end case;
            end if;

            -- C. LOGICA FISICA ENCODER (Usa segnali DEBOUNCED)
            for i in 0 to 6 loop
                -- Rileva fronte di salita sul segnale PULITO (deb_a)
                if deb_a(i) = '1' and last_a(i) = '0' then 
                    
                    if deb_b(i) = '0' then 
                        next_v := bank(i).c_val + bank(i).step_v;
                    else                     
                        next_v := bank(i).c_val - bank(i).step_v;
                    end if;
                    
                    -- Saturazione Min/Max
                    if next_v > bank(i).max_v then 
                        bank(i).c_val <= bank(i).max_v;
                    elsif next_v < bank(i).min_v then 
                        bank(i).c_val <= bank(i).min_v;
                    else 
                        bank(i).c_val <= next_v;
                    end if;
                end if;
            end loop;

            -- Aggiorna lo stato precedente per il rilevamento fronte
            last_a <= deb_a; 
        end if;
    end process;

    -- 3. LOGICA DI LETTURA (Output combinatorio)
    process(addr_in, bank, sel_enc)
    begin
        case addr_in is
            when ENC_DATA_HI => data_out <= std_logic_vector(bank(sel_enc).c_val(15 downto 8));
            when ENC_DATA_LO => data_out <= std_logic_vector(bank(sel_enc).c_val(7 downto 0));
            when others      => data_out <= (others => '0');
        end case;
    end process;

end Behavioral;