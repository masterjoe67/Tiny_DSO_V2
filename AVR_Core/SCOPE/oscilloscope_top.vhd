--************************************************************************************************
-- Top entity for DSO
-- Version 0.5 (Version for Intel)
-- Designed by Giovanni Legati
-- M.J.E. 2026
-- masterjoe67@hotmail.it
--************************************************************************************************
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.AVRuCPackage.all;

entity oscilloscope_top is
    port(
        clk           : in  std_logic;
        rst_n         : in  std_logic;

        -- SPI ADC
        sclk          : out std_logic;
        cs_n          : out std_logic;
        miso          : in  std_logic;
        mosi          : out std_logic;

        -- MMIO interface
        iore          : in  std_logic;
        mmio_addr     : in  std_logic_vector(6 downto 0);
        mmio_wdata    : in  std_logic_vector(7 downto 0);
        mmio_we       : in  std_logic;
        mmio_rdata    : out std_logic_vector(7 downto 0);
        out_en        : out std_logic;
		  tb_view_full_sign : out std_logic_vector(15 downto 0)

    );
end entity;

architecture rtl of oscilloscope_top is

    ------------------------------------------------------------------
    -- Tipi e Costanti Time/Div
    ------------------------------------------------------------------
    type time_div_map_t is array (0 to 19) of unsigned(31 downto 0);

    constant time_div_map : time_div_map_t := (
        to_unsigned(0, 32),        -- 0: 1us
        to_unsigned(1, 32),        -- 1: 2us
        to_unsigned(4, 32),        -- 2: 5us
        to_unsigned(9, 32),        -- 3: 10us
        to_unsigned(19, 32),       -- 4: 20us
        to_unsigned(49, 32),       -- 5: 50us
        to_unsigned(99, 32),       -- 6: 100us
        to_unsigned(199, 32),      -- 7: 200us
        to_unsigned(499, 32),      -- 8: 500us
        to_unsigned(999, 32),      -- 9: 1ms
        to_unsigned(1999, 32),     -- 10: 2ms
        to_unsigned(4999, 32),     -- 11: 5ms
        to_unsigned(9999, 32),     -- 12: 10ms
        to_unsigned(19999, 32),    -- 13: 20ms
        to_unsigned(49999, 32),    -- 14: 50ms
        to_unsigned(99999, 32),    -- 15: 100ms
        to_unsigned(199999, 32),   -- 16: 200ms
        to_unsigned(499999, 32),   -- 17: 500ms
        to_unsigned(999999, 32),   -- 18: 1s
        to_unsigned(1999999, 32)   -- 19: 2s
    );

    ------------------------------------------------------------------
    -- Costanti di sistema
    ------------------------------------------------------------------
    constant BUFFER_SIZE      : integer := 4096;
	 constant PTR_BITS         : integer := 12;
    constant PRE_TRIGGER      : integer := 150;
    constant POST_TRIGGER_LEN : integer := 255;
    constant AUTO_TIMEOUT     : unsigned(15 downto 0) := to_unsigned(2050, 16);
	 constant PAN_LIMIT : integer := BUFFER_SIZE/2;

    ------------------------------------------------------------------
    -- Segnali Interni
    ------------------------------------------------------------------
    signal base_time_reload      : unsigned(31 downto 0);
    signal next_base_time_reload : unsigned(31 downto 0);
    signal reg_time_div_sel      : integer range 0 to 19 := 0;
    signal rd_base_stable        : unsigned(PTR_BITS-1 downto 0);

    -- Registri MMIO
    signal reg_index_int         : unsigned(9 downto 0)  := (others => '0');
    signal reg_base_time         : unsigned(31 downto 0) := (others => '0');
    signal reg_trig_level        : unsigned(11 downto 0) := (others => '0');
    signal reg_trig_ctrl         : std_logic_vector(7 downto 0) := (others => '0');

    -- Decode MMIO
    signal index_reg_sel         : std_logic;
    signal trig_reg_sel          : std_logic;
    signal base_reg_sel          : std_logic;
    signal trig_ctrl_sel         : std_logic;
    signal trig_cmd_sel          : std_logic;

    -- FSM
    type fsm_state_t is (IDLE, PRE_FILL, ARMED, POST_TRIGGER, HOLD);
    signal state, next_state     : fsm_state_t;

    -- Contatori e puntatori
    signal post_cnt              : unsigned(9 downto 0) := (others => '0');
    signal rd_index              : unsigned(PTR_BITS-1 downto 0);
    signal pre_cnt               : unsigned(9 downto 0);
    signal auto_cnt              : unsigned(15 downto 0);
    signal wr_ptr                : unsigned(PTR_BITS-1 downto 0);
    signal trig_index            : unsigned(PTR_BITS-1 downto 0);
    signal rd_base_latched       : unsigned(PTR_BITS-1 downto 0) := (others => '0');
    signal trig_wr_ptr           : unsigned(PTR_BITS-1 downto 0) := (others => '0');
    signal rd_cha_strobe         : std_logic := '0';
    signal ready_latched         : std_logic := '0';

    -- Base tempi / tick
    signal base_time_cnt         : unsigned(31 downto 0) := (others => '0');
    signal tick                  : std_logic := '0';
    signal tick_en               : std_logic := '0';

    -- Trigger / controllo
    signal trig_hit              : std_logic;
    signal mode                  : std_logic_vector(1 downto 0);
    signal write_enable          : std_logic;
    signal save_trig             : std_logic;
    signal freeze                : std_logic;
    signal ready                 : std_logic := '0';
    signal rearm_pulse           : std_logic := '0';
    signal auto_timeout_hit      : std_logic;
    signal trig_occurred         : std_logic := '0';
    signal trig_armed            : std_logic := '0';

    -- ADC samples
    signal trig_sample           : unsigned(9 downto 0);
    signal prev_sample           : unsigned(9 downto 0);
    signal trig_sample_sync      : unsigned(9 downto 0);

    -- MMIO byte counters
    signal base_bytecnt          : unsigned(2 downto 0);
    signal base_shift            : unsigned(31 downto 0);
    signal trig_bytecnt          : unsigned(1 downto 0);
    signal trig_shift            : unsigned(23 downto 0);
    signal wr_timeout            : unsigned(15 downto 0) := (others => '0');

    -- ADC channels / RAM signals
    signal adc_a, adc_b, adc_c   : unsigned(11 downto 0);
    signal ram_a_out, ram_b_out, ram_c_out : unsigned(11 downto 0);

    -- Trigger configuration
    signal trig_level            : unsigned(11 downto 0);
    signal trig_chan_sel         : std_logic_vector(1 downto 0);
    signal trig_edge             : std_logic;
    signal trig_enable           : std_logic;

    -- ADC reader signals (non usati ma mantenuti per logica)
    signal adc_start             : std_logic := '0';
    signal adc_tick              : std_logic := '0';
    signal adc_div               : unsigned(15 downto 0) := (others => '0');
	 

	 signal base_cmd          : std_logic := '0'; -- 0=time_div, 1=view_offset
	 signal view_bytecnt      : unsigned(2 downto 0) := (others => '0');
	 signal view_offset       : signed(PTR_BITS-1 downto 0) := (others => '0');
	 signal view_shift : signed(15 downto 0) := (others => '0');
	 
	 signal view_full_raw : std_logic_vector(15 downto 0) := (others => '0');
	 signal view_full_sign       : signed(15 downto 0) := (others => '0');
    ------------------------------------------------------------------
    -- Utility function
    ------------------------------------------------------------------
    function wrap_sub(a, b : unsigned; size : integer) return unsigned is
    begin
        if a < b then
            return a + to_unsigned(size - to_integer(b), a'length);
        else
            return a - b;
        end if;
    end function;

begin

    ------------------------------------------------------------------
    -- MMIO register select
    -- Decodifica indirizzi MMIO per scrittura registri interni
    ------------------------------------------------------------------
    index_reg_sel <= '1' when (mmio_addr = REG_INDEX and mmio_we = '1') else '0';
    trig_reg_sel  <= '1' when (mmio_addr = REG_CHA   and mmio_we = '1') else '0';
    base_reg_sel  <= '1' when (mmio_addr = REG_CHB   and mmio_we = '1') else '0';
    trig_ctrl_sel <= '1' when (mmio_addr = REG_CHC   and mmio_we = '1') else '0';
    trig_cmd_sel  <= '1' when (mmio_addr = REG_TRIG  and mmio_we = '1') else '0';

    ------------------------------------------------------------------
    -- Read index calculation
    -- Calcolo indice di lettura RAM con base stabile
    ------------------------------------------------------------------
    --rd_index <= rd_base_stable + reg_index_int;
	 rd_index <= unsigned(
               signed(rd_base_stable) +
               view_offset +
               signed(reg_index_int)
           );

    ------------------------------------------------------------------
    -- Trigger sample selection
    -- Selezione canale ADC per la comparazione di trigger
    ------------------------------------------------------------------
    with trig_chan_sel select
        trig_sample <= adc_a(11 downto 2) when "00",
                       adc_b(11 downto 2) when "01",
                       adc_c(11 downto 2) when "10",
                       (others => '0')    when others;

    mode          <= reg_trig_ctrl(7 downto 6);
    trig_chan_sel <= reg_trig_ctrl(5 downto 4);
    trig_edge     <= reg_trig_ctrl(3);
    trig_enable   <= reg_trig_ctrl(2);
    trig_level    <= reg_trig_level;
    ready         <= '1' when state = HOLD else '0';



    auto_timeout_hit <= '1' when (mode = "00") and (auto_cnt >= AUTO_TIMEOUT) else '0';
    next_base_time_reload <= time_div_map(reg_time_div_sel);
	 
	 tb_view_full_sign <= std_logic_vector(view_full_sign);

    ------------------------------------------------------------------
    -- Stable read base logic
    -- Aggiorna la base di lettura solo al termine dell'acquisizione
    ------------------------------------------------------------------
    process(clk, rst_n)
    begin
        if rst_n = '0' then
            rd_base_stable <= (others => '0');
        elsif rising_edge(clk) then
            if state = POST_TRIGGER and next_state = HOLD then
                rd_base_stable <= rd_base_latched;
            end if;
        end if;
    end process;

    ------------------------------------------------------------------
    -- Trigger occurrence latch
    -- Memorizza l'evento di trigger durante lo stato ARMED
    ------------------------------------------------------------------
    process(clk, rst_n)
    begin
        if rst_n = '0' then
            trig_occurred <= '0';
        elsif rising_edge(clk) then
            if state = ARMED and trig_hit = '1' then
                trig_occurred <= '1';
            elsif state = HOLD then
                trig_occurred <= '0';
            end if;
        end if;
    end process;

    ------------------------------------------------------------------
    -- Rearm command latch
    -- Genera un impulso di rearm di un ciclo clock
    ------------------------------------------------------------------
    process(clk, rst_n)
    begin
        if rst_n = '0' then
            rearm_pulse <= '0';
        elsif rising_edge(clk) then
            rearm_pulse <= '0';
            if mmio_we = '1' and trig_cmd_sel = '1' and mmio_wdata(0) = '1' then
                rearm_pulse <= '1';
            end if;
        end if;
    end process;

    ------------------------------------------------------------------
    -- ADC sample synchronization
    -- Gestione campioni per il rilevamento del fronte di trigger
    ------------------------------------------------------------------
    process(clk, rst_n)
    begin
        if rst_n = '0' then
            trig_sample_sync <= (others => '0');
            prev_sample      <= (others => '0');
        elsif rising_edge(clk) then
            trig_sample_sync <= trig_sample;
            if tick_en = '1' and write_enable = '1' then
                prev_sample <= trig_sample_sync;
            end if;
        end if;
    end process;

    ------------------------------------------------------------------
    -- Trigger detection
    -- Rilevamento fronte con isteresi e armamento logico
    ------------------------------------------------------------------
    process(clk, rst_n)
        constant HYST : unsigned(11 downto 0) := to_unsigned(20, 12);
    begin
        if rst_n = '0' then
            trig_hit    <= '0';
            trig_wr_ptr <= (others => '0');
            trig_armed  <= '0';
        elsif rising_edge(clk) then
            trig_hit <= '0';
            if trig_enable = '1' and state = ARMED then
                if trig_edge = '0' then -- Rising Edge
                    if unsigned(trig_sample_sync) < (unsigned(trig_level) - HYST) then
                        trig_armed <= '1';
                    end if;
                    if trig_armed = '1' and (prev_sample < trig_level) 
                                        and (trig_sample_sync >= trig_level) then
                        trig_hit    <= '1';
                        trig_wr_ptr <= wr_ptr;
                        trig_armed  <= '0';
                    end if;
                else -- Falling Edge
                    if unsigned(trig_sample_sync) > (unsigned(trig_level) + HYST) then
                        trig_armed <= '1';
                    end if;
                    if trig_armed = '1' and (prev_sample > trig_level) 
                                        and (trig_sample_sync <= trig_level) then
                        trig_hit    <= '1';
                        trig_wr_ptr <= wr_ptr;
                        trig_armed  <= '0';
                    end if;
                end if;
            else
                trig_armed <= '0';
            end if;
        end if;
    end process;

    ------------------------------------------------------------------
    -- Tick generator
    -- Genera tick_en basato sul Time/Div selezionato
    ------------------------------------------------------------------
    process(clk, rst_n)
    begin
        if rst_n = '0' then
            tick             <= '0';
            tick_en          <= '0';
            base_time_cnt    <= (others=>'0');
            base_time_reload <= (others => '0');
        elsif rising_edge(clk) then
            if state /= next_state then
                base_time_cnt <= (others => '0');
                tick          <= '0';
                tick_en       <= '0';
            end if;

            if state = IDLE or state = HOLD then
                if base_time_reload /= time_div_map(reg_time_div_sel) then
                    base_time_reload <= time_div_map(reg_time_div_sel);
                    base_time_cnt    <= (others => '0');
                end if;
            end if;
            
            if base_time_cnt >= base_time_reload then
                base_time_cnt <= (others=>'0');
                tick          <= '1';
                tick_en       <= '1';
            else
                base_time_cnt <= base_time_cnt + 1;
                tick          <= '0';
                tick_en       <= '0';
            end if;
        end if;
    end process;

    ------------------------------------------------------------------
    -- Write pointer logic
    -- Gestisce l'indirizzo di scrittura circolare della RAM
    ------------------------------------------------------------------
    process(clk, rst_n)
    begin
        if rst_n = '0' then
            wr_ptr <= (others => '0');
        elsif rising_edge(clk) then
            if tick_en = '1' and write_enable = '1' and state /= HOLD then
                if wr_ptr = to_unsigned(BUFFER_SIZE - 1, wr_ptr'length) then
                    wr_ptr <= (others => '0');
                else
                    wr_ptr <= wr_ptr + 1;
                end if;
            end if;
        end if;
    end process;

    ------------------------------------------------------------------
    -- Dual-port RAM instantiation (3 canali)
    ------------------------------------------------------------------
    ram_a : entity work.dp_ram_4096x12
        port map(
            clk_wr   => clk, clk_rd   => clk,
            wr_en    => write_enable,
            addr_wr  => wr_ptr,
            data_in  => adc_a,
            addr_rd  => rd_index,
            data_out => ram_a_out
        );

    ram_b : entity work.dp_ram_4096x12
        port map(
            clk_wr   => clk, clk_rd   => clk,
            wr_en    => write_enable,
            addr_wr  => wr_ptr,
            data_in  => adc_b,
            addr_rd  => rd_index,
            data_out => ram_b_out
        );

    ram_c : entity work.dp_ram_4096x12
        port map(
            clk_wr   => clk, clk_rd   => clk,
            wr_en    => write_enable,
            addr_wr  => wr_ptr,
            data_in  => adc_c,
            addr_rd  => rd_index,
            data_out => ram_c_out
        );

------------------------------------------------------------------
-- MMIO write logic - Versione Corretta
------------------------------------------------------------------
process(clk, rst_n)
begin
    if rst_n = '0' then
        reg_index_int    <= (others => '0');
        reg_trig_level   <= to_unsigned(512, 12);
        reg_trig_ctrl    <= (others => '0');
        base_bytecnt     <= (others => '0');
        base_shift       <= (others => '0');
        trig_shift       <= (others => '0');
        trig_bytecnt     <= (others => '0');
        reg_base_time    <= to_unsigned(20000, 32);
        wr_timeout       <= (others => '0');
        reg_time_div_sel <= 0;
        view_bytecnt     <= (others => '0');
        view_offset      <= (others => '0');
        view_shift       <= (others => '0');
        view_full_raw    <= (others => '0');
        base_cmd         <= '0';

    elsif rising_edge(clk) then
        if mmio_we = '1' then
            wr_timeout <= to_unsigned(20000, wr_timeout'length);
        elsif wr_timeout /= 0 then
            wr_timeout <= wr_timeout - 1;
        else
            trig_bytecnt <= (others => '0');
            base_bytecnt <= (others => '0');
            view_bytecnt <= (others => '0'); 
        end if;

        if index_reg_sel = '1' and mmio_we = '1' then
            reg_index_int <= "00" & unsigned(mmio_wdata);
        elsif rd_cha_strobe = '1' then
            reg_index_int <= reg_index_int + 1;
        end if;

        if mmio_we = '1' and base_reg_sel = '1' then
				-- *** PRIORITÀ 1: CONCLUSIONE CICLO (Stato 100) ***
            -- Se siamo arrivati qui, view_full_raw ha già LSB e MSB salvati.
            -- Questo ciclo di clock corrisponde al tuo "REG_BASETIME = 0xFF" finale.
            if view_bytecnt = "100" then
                -- ESEGUIAMO IL CALCOLO ORA!
                -- view_full_raw è stabile da un ciclo, quindi niente errori.

					 
					 
					 view_full_sign <= signed(view_full_raw);
                view_offset <= resize(signed(view_full_raw), view_offset'length);
                
                -- Poiché l'ultimo byte è 0xFF (Escape), il prossimo stato logico
                -- è "001" (siamo già in modalità comando), non "000".
                view_bytecnt <= "000";
				-- RICEZIONE DATI: Qui NON dobbiamo controllare x"FF"
            elsif view_bytecnt = "010" then
                view_full_raw(7 downto 0) <= mmio_wdata; -- Salva LSB
                view_bytecnt <= "011";
					 
				elsif view_bytecnt = "011" then
                view_full_raw(15 downto 8) <= mmio_wdata; -- Salva MSB (il tuo 0xFF!)
                view_bytecnt <= "100";
					 
				-- COMANDI: Qui controlliamo x"FF" per iniziare la sequenza	 
            elsif mmio_wdata = x"FF" then
                view_bytecnt <= "001";

            elsif view_bytecnt = "001" then
                base_cmd     <= mmio_wdata(0);
                view_bytecnt <= "010";
					 
			
            else
                reg_time_div_sel <= to_integer(unsigned(mmio_wdata(4 downto 0)));
            end if;
        end if;

        -- Logica Trigger (Invariata)
        if mmio_we = '1' and trig_reg_sel = '1' then
            if trig_bytecnt = "00" then trig_shift(7 downto 0) <= unsigned(mmio_wdata);
            elsif trig_bytecnt = "01" then trig_shift(15 downto 8) <= unsigned(mmio_wdata);
            elsif trig_bytecnt = "10" then
                trig_shift(23 downto 16) <= unsigned(mmio_wdata);
                reg_trig_level <= trig_shift(11 downto 0);
            end if;
            if trig_bytecnt = "10" then trig_bytecnt <= (others => '0');
            else trig_bytecnt <= trig_bytecnt + 1; end if;
        end if;

        if mmio_we = '1' and trig_ctrl_sel = '1' then
            reg_trig_ctrl <= mmio_wdata;
        end if;
    end if;
end process;

    ------------------------------------------------------------------
    -- MMIO read logic
    -- Gestione letture registri e auto-incremento indice
    ------------------------------------------------------------------
    process(mmio_addr, iore)
    begin
        mmio_rdata    <= (others => '0');
        out_en        <= '0';
        rd_cha_strobe <= '0';
        if iore = '1' then
            case mmio_addr is
                when REG_CHA =>
                    mmio_rdata <= std_logic_vector(to_unsigned(to_integer(ram_a_out) / 16, 8));
                    out_en        <= '1';
                    rd_cha_strobe <= '1';
                when REG_CHB =>
                    mmio_rdata <= std_logic_vector(to_unsigned(to_integer(ram_b_out) / 16, 8));
                    out_en <= '1';
                when REG_CHC =>
                    mmio_rdata <= std_logic_vector(to_unsigned(to_integer(ram_c_out) / 16, 8));
                    out_en <= '1';
                when REG_INDEX =>
                    mmio_rdata <= std_logic_vector(reg_index_int(7 downto 0));
                    out_en <= '1';
                when REG_TRIG =>
                    mmio_rdata <= reg_trig_ctrl or ("000000" & ready & '0');
                    out_en <= '1';
                when others =>
                    out_en <= '0';
            end case;
        end if;
    end process;

    ------------------------------------------------------------------
    -- FSM state register
    -- Registro di stato principale
    ------------------------------------------------------------------
    process(clk, rst_n)
    begin
        if rst_n = '0' then
            state <= IDLE;
        elsif rising_edge(clk) then
            state <= next_state;
        end if;
    end process;

    ------------------------------------------------------------------
    -- READY latch
    -- Sticky flag fino alla lettura del registro trigger
    ------------------------------------------------------------------
    process(clk, rst_n)
    begin
        if rst_n = '0' then
            ready_latched <= '0';
        elsif rising_edge(clk) then
            if state = HOLD then
                ready_latched <= '1';
            elsif mmio_addr = REG_TRIG and iore = '1' then
                ready_latched <= '0';
            end if;
        end if;
    end process;

    ------------------------------------------------------------------
    -- FSM next state logic
    -- Logica di transizione degli stati dell'oscilloscopio
    ------------------------------------------------------------------
    process(state, trig_occurred, auto_timeout_hit, pre_cnt, post_cnt, rearm_pulse, tick_en)
    begin
        next_state <= state;
        case state is
            when IDLE =>
                next_state <= PRE_FILL;
            when PRE_FILL =>
                if tick_en = '1' and pre_cnt >= to_unsigned(PRE_TRIGGER-1, 10) then
                    next_state <= ARMED;
                end if;
            when ARMED =>
                if (trig_occurred = '1' or auto_timeout_hit = '1') then
                    next_state <= POST_TRIGGER;
                end if;
            when POST_TRIGGER =>
                if tick_en = '1' and post_cnt >= to_unsigned(POST_TRIGGER_LEN-1, 10) then
                    next_state <= HOLD;
                end if;
            when HOLD =>
                if rearm_pulse = '1' then
                    next_state <= IDLE;
                end if;
            when others =>
                next_state <= IDLE;
        end case;
    end process;

    ------------------------------------------------------------------
    -- save_trig pulse
    -- Genera l'impulso per catturare i puntatori al momento del trigger
    ------------------------------------------------------------------
    process(clk, rst_n)
    begin
        if rst_n = '0' then
            save_trig <= '0';
        elsif rising_edge(clk) then
            if state = ARMED and next_state = POST_TRIGGER then
                save_trig <= '1';
            else
                save_trig <= '0';
            end if;
        end if;
    end process;

    ------------------------------------------------------------------
    -- Write enable / freeze logic
    -- Abilita la scrittura in RAM negli stati attivi
    ------------------------------------------------------------------
    process(state)
    begin
        write_enable <= '0';
        freeze       <= '0';
        case state is
            when PRE_FILL | ARMED | POST_TRIGGER =>
                write_enable <= '1';
            when HOLD =>
                freeze <= '1';
                write_enable <= '0';
            when others =>
                write_enable <= '0';
        end case;
    end process;

    ------------------------------------------------------------------
    -- PRE / POST / AUTO counters
    -- Contatori di campionamento e timeout auto-trigger
    ------------------------------------------------------------------
    process(clk, rst_n)
    begin
        if rst_n = '0' then
            pre_cnt  <= (others => '0');
            post_cnt <= (others => '0');
            auto_cnt <= (others => '0');
        elsif rising_edge(clk) then
            if state /= next_state then
                pre_cnt  <= (others => '0');
                post_cnt <= (others => '0');
                auto_cnt <= (others => '0');
            elsif tick_en = '1' then
                case state is
                    when PRE_FILL =>
                        pre_cnt <= pre_cnt + 1;
                    when ARMED =>
                        auto_cnt <= auto_cnt + 1;
                    when POST_TRIGGER =>
                        post_cnt <= post_cnt + 1;
                    when others =>
                        null;
                end case;
            end if;
        end if;
    end process;

    ------------------------------------------------------------------
    -- Trigger index and read base latch
    -- Calcola il punto di inizio visualizzazione buffer
    ------------------------------------------------------------------
    process(clk, rst_n)
    begin
        if rst_n = '0' then
            trig_index      <= (others => '0');
            rd_base_latched <= (others => '0');
        elsif rising_edge(clk) then
            if save_trig = '1' then
                trig_index      <= trig_wr_ptr; 
                rd_base_latched <= trig_wr_ptr - to_unsigned(PRE_TRIGGER, 10);
            end if;
        end if;
    end process;

    ------------------------------------------------------------------
    -- ADC reader instantiation
    ------------------------------------------------------------------
    adc_reader_inst : entity work.adc128s022_reader
        port map(
            clk   => clk,   rst_n => rst_n,
            miso  => miso,  mosi  => mosi,
            sclk  => sclk,  cs_n  => cs_n,
            ch0   => adc_a, ch1   => adc_b, ch2   => adc_c
        );

end architecture;