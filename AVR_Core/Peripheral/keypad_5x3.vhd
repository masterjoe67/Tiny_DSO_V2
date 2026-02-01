library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.AVRuCPackage.all;

entity keypad_5x3 is
    port (
        clk             : in  std_logic;
        rst_n           : in  std_logic;

        -- GPIO
        rows_i          : in  std_logic_vector(4 downto 0); -- 5 INGRESSI (pull-up)
        cols_o          : out std_logic_vector(2 downto 0); -- 3 USCITE

        -- MMIO
        mmio_we         : in  std_logic;
        mmio_re         : in  std_logic;
        mmio_addr       : in  std_logic_vector(6 downto 0);
        mmio_wdata      : in  std_logic_vector(7 downto 0);
        mmio_rdata      : out std_logic_vector(7 downto 0);
        out_en          : out std_logic;
        tb_key_ctrl_Sel : out std_logic
    );
end entity;

architecture rtl of keypad_5x3 is
    signal col_idx             : integer range 0 to 2 := 0;
    signal scan_tick           : std_logic := '0';
    signal key_valid           : std_logic := '0';
    signal key_code_latched    : unsigned(3 downto 0) := (others => '0');
    signal key_event_locked    : std_logic := '0';
    signal debounce_cnt        : integer range 0 to 2047 := 0;
    signal keys_found_in_round : std_logic := '0'; 
    signal repeat_en           : std_logic := '0';

begin

    -- 1. PROCESSO LOGICO (Sequenziale)
    process(clk)
        variable cnt : integer range 0 to 49999 := 0;
    begin
        if rising_edge(clk) then
            if rst_n = '0' then
                cnt := 0;
                col_idx <= 0;
                cols_o <= "110";
                key_valid <= '0';
                key_event_locked <= '0';
                keys_found_in_round <= '0';
                debounce_cnt <= 0;
                repeat_en <= '0';
                key_code_latched <= (others => '0');
            else
                -- SCANSIONE COLONNE
                if cnt = 49999 then
                    cnt := 0;
                    if col_idx = 2 then
                        col_idx <= 0;
                        cols_o <= "110";
                        if keys_found_in_round = '0' then
                            key_event_locked <= '0';
                        end if;
                        keys_found_in_round <= '0';
                    else
                        col_idx <= col_idx + 1;
                        if col_idx = 0 then 
                            cols_o <= "101"; 
                        else 
                            cols_o <= "011"; 
                        end if;
                    end if;
                else
                    cnt := cnt + 1;
                end if;

                -- RILEVAMENTO PRESSIONE
                if rows_i /= "11111" then
                    keys_found_in_round <= '1';
                    if key_event_locked = '0' then
                        if debounce_cnt < 1000 then
                            debounce_cnt <= debounce_cnt + 1;
                        else
                            key_valid <= '1';
                            key_event_locked <= '1'; 
                            -- Mapping
                            if    rows_i(0)='0' then key_code_latched <= to_unsigned(0 + col_idx, 4);
                            elsif rows_i(1)='0' then key_code_latched <= to_unsigned(3 + col_idx, 4);
                            elsif rows_i(2)='0' then key_code_latched <= to_unsigned(6 + col_idx, 4);
                            elsif rows_i(3)='0' then key_code_latched <= to_unsigned(9 + col_idx, 4);
                            elsif rows_i(4)='0' then key_code_latched <= to_unsigned(12+ col_idx, 4);
                            end if;
                        end if;
                    end if;
                else
                    debounce_cnt <= 0;
                end if;

                -- ACK MMIO (Reset automatico su lettura)
                if mmio_re = '1' and mmio_addr = KEY_STATUS then
                    key_valid <= '0';
                end if;

                -- SCRITTURA CONFIG
                if mmio_we = '1' and mmio_addr = KEY_CTRL then
                    repeat_en <= mmio_wdata(0);
                end if;
            end if;
        end if;
    end process;

    -- 2. PROCESSO BUS (Combinatorio)
    process(mmio_addr, key_valid, key_code_latched, repeat_en, mmio_re)
    begin
        -- Valori di default per evitare latch indesiderati
        out_en <= '0';
        mmio_rdata <= (others => '0');
        tb_key_ctrl_Sel <= '0';
        
        if mmio_re = '1' then
            case mmio_addr is
                when KEY_STATUS => 
                    out_en <= '1';
                    mmio_rdata(0) <= key_valid;
                when KEY_CODE => 
                    out_en <= '1';
                    mmio_rdata(3 downto 0) <= std_logic_vector(key_code_latched);
                when KEY_CTRL => 
                    out_en <= '1';
                    mmio_rdata(0) <= repeat_en;
                when others => 
                    out_en <= '0';
            end case;
        end if;
        
        -- Assegnazione separata fuori dal case per chiarezza
        if mmio_addr = KEY_CTRL then
            tb_key_ctrl_Sel <= '1';
        end if;
    end process;

end architecture;