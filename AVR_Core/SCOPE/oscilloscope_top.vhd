--************************************************************************************************
-- Top entity for DSO logic
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

		-- Interfaccia Modulo ADC Canale A
		DATA_A           : in  std_logic_vector(11 downto 0); -- A1..12
		ACK         : out std_logic;                     -- Clock A
		ORA         : in  std_logic;                     -- Out of Range A

		-- Interfaccia Modulo ADC Canale B
		DATA_B           : in  std_logic_vector(11 downto 0); -- B1..12
		BCK         : out std_logic;                     -- Clock B
		ORB         : in  std_logic;                     -- Out of Range B

		-- MMIO interface
		iore          : in  std_logic;
		mmio_addr     : in  std_logic_vector(6 downto 0);
		mmio_wdata    : in  std_logic_vector(7 downto 0);
		mmio_we       : in  std_logic;
		mmio_rdata    : out std_logic_vector(7 downto 0);
		out_en        : out std_logic;
		tb_view_full_sign : out std_logic_vector(15 downto 0);

		-- RAM interface
		bram_ce          : in  std_logic;
		mem_ramadr		 : in std_logic_vector(15 downto 0);
		ramre				 : in  std_logic;
		ramwe				 : in  std_logic;
		data_in			 : in std_logic_vector(7 downto 0);
		data_out			 : out std_logic_vector(7 downto 0)
		  

    );
end entity;

architecture rtl of oscilloscope_top is


	 ------------------------------------------------------------------
    -- Tipi e Costanti Time/Div (Ricalcolate per 40 px/div @ 60MHz)
    ------------------------------------------------------------------
    type time_div_map_t is array (0 to 19) of unsigned(31 downto 0);

    -- Formula: (Tempo_Divisione / (16.66ns * 40)) - 1
    constant time_div_map : time_div_map_t := (
        to_unsigned(0, 32),          -- 0: ~666ns/div (Max speed: 1 tick/clock)
        to_unsigned(2, 32),          -- 1: 2us/div    (60MHz / 3 = 20MHz -> 40px * 50ns)
        to_unsigned(6, 32),          -- 2: 5us/div    
        to_unsigned(14, 32),         -- 3: 10us/div
        to_unsigned(29, 32),         -- 4: 20us/div
        to_unsigned(74, 32),         -- 5: 50us/div
        to_unsigned(149, 32),        -- 6: 100us/div
        to_unsigned(299, 32),        -- 7: 200us/div
        to_unsigned(749, 32),        -- 8: 500us/div
        to_unsigned(1499, 32),       -- 9: 1ms/div
        to_unsigned(2999, 32),       -- 10: 2ms/div
        to_unsigned(7499, 32),       -- 11: 5ms/div
        to_unsigned(14999, 32),      -- 12: 10ms/div
        to_unsigned(29999, 32),      -- 13: 20ms/div
        to_unsigned(74999, 32),      -- 14: 50ms/div
        to_unsigned(149999, 32),     -- 15: 100ms/div
        to_unsigned(299999, 32),     -- 16: 200ms/div
        to_unsigned(749999, 32),     -- 17: 500ms/div
        to_unsigned(1499999, 32),    -- 18: 1s/div
        to_unsigned(2999999, 32)     -- 19: 2s/div
    );

    ------------------------------------------------------------------
    -- Costanti di sistema
    ------------------------------------------------------------------
    constant BUFFER_SIZE      : integer := 4096;
	 constant PTR_BITS         : integer := 12;
    constant PRE_TRIGGER      : integer := 1024;
    constant POST_TRIGGER_LEN : integer := 1024;
    constant AUTO_TIMEOUT     : unsigned(15 downto 0) := to_unsigned(2050, 16);
	 constant PAN_LIMIT 			: integer := BUFFER_SIZE/2;
	 constant VIEW_BEGIN      : integer := 200;

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
	 
	 signal reg_trig_hyst : unsigned(11 downto 0) := to_unsigned(20, 12); -- Default 20

    -- ADC samples
    signal trig_sample           : unsigned(11 downto 0);
    signal prev_sample           : unsigned(11 downto 0);
    signal trig_sample_sync      : unsigned(11 downto 0);

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
	 
	 signal freq_counter    : unsigned(31 downto 0) := (others => '0');
	 signal freq_period_reg : unsigned(31 downto 0) := (others => '0');
	 signal freq_period_latch : unsigned(31 downto 0);
	 signal freq_byte_idx     : unsigned(1 downto 0) := "00";
	 signal trig_hit_raw    : std_logic; -- Il trigger "puro", sempre attivo per il frequenzimetro
	 signal prev_sample_raw : unsigned(11 downto 0); -- Campione precedente per il trigger raw
	 signal freq_trig_armed : std_logic := '0'; -- Stato di armamento per il trigger raw
	 
	 signal rd_addr_full : signed(PTR_BITS downto 0);
	 
	 signal gate_count   : unsigned(7 downto 0) := (others => '0');
	signal accumulator  : unsigned(31 downto 0) := (others => '0');
	signal v_trig_armed : std_logic := '0';
	signal trig_prev_val : unsigned(11 downto 0) := (others => '0');
	signal reg_st_addr : unsigned(PTR_BITS-1 downto 0) := (others => '0');

	signal inc_ptr_pulse : std_logic;
	signal bram_data_to_avr : std_logic_vector(7 downto 0);
	signal last_ramre : std_logic;
	signal ctrl_reset_index : std_logic := '0';
	signal rst_cmd_sel : std_logic := '0';
	
-- Segnali di appoggio coprocessore
    signal scale_ch1, scale_ch2   : signed(15 downto 0) := (others => '0');
    signal offset_ch1, offset_ch2 : signed(15 downto 0) := (others => '0');
    signal adc_temp_l             : std_logic_vector(7 downto 0);
    signal y_result               : signed(15 downto 0) := (others => '0');
    signal adc_full_12bit         : signed(15 downto 0);
	 
	 -- Nuovi segnali per DDS a 32 bit
    signal reg_dds_inc     : unsigned(31 downto 0) := (others => '0');
    signal dds_temp        : unsigned(23 downto 0) := (others => '0');
    signal dds_bytecnt     : integer range 0 to 3  := 0;
    signal dds_acc         : unsigned(31 downto 0) := (others => '0');
    signal tick_dds        : std_logic := '0';

    -- Attributi per visibilità Signal Tap
    attribute keep : boolean;
    attribute noprune: boolean;
    
    attribute keep of offset_ch1, offset_ch2 : signal is true;
    attribute noprune of offset_ch1, offset_ch2 : signal is true;
    attribute keep of adc_temp_l, y_result : signal is true;
    attribute noprune of adc_temp_l, y_result : signal is true;
    attribute keep of scale_ch1, scale_ch2 : signal is true;
    attribute noprune of scale_ch1, scale_ch2 : signal is true;
    attribute noprune of adc_full_12bit : signal is true;
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
--	inc_ptr_pulse <= '1' when (bram_ce = '1' and mem_ramadr(1 downto 0) = "11" and ramre = '1') 
--                 else '0';
	data_out <= bram_data_to_avr;
    ------------------------------------------------------------------
    -- MMIO register select
    -- Decodifica indirizzi MMIO per scrittura registri interni
    ------------------------------------------------------------------
    index_reg_sel <= '1' when (mmio_addr = REG_INDEX and mmio_we = '1') else '0';
    trig_reg_sel  <= '1' when (mmio_addr = REG_CHA   and mmio_we = '1') else '0';
    base_reg_sel  <= '1' when (mmio_addr = REG_CHB   and mmio_we = '1') else '0';
    trig_ctrl_sel <= '1' when (mmio_addr = REG_TG_CTRL   and mmio_we = '1') else '0';
    trig_cmd_sel  <= '1' when (mmio_addr = REG_TRIG  and mmio_we = '1') else '0';
	 rst_cmd_sel  <= '1' when (mmio_addr = REG_RESET_INDEX  and mmio_we = '1') else '0';
	 
	 
	 
	 -- Coprocessore

	
	-- Processo di scrittura dal BUS (AVR -> FPGA)
process(clk)
    variable v_adc   : signed(15 downto 0);
    variable v_mult  : signed(31 downto 0);
begin
    if rising_edge(clk) then
        if (ramwe = '1') then
            case mem_ramadr is
                -- CONFIGURAZIONE CH1 (0x4010 - 0x4013)
                when x"4010" => scale_ch1(7 downto 0)  <= signed(data_in);
                when x"4011" => scale_ch1(15 downto 8) <= signed(data_in);
                when x"4012" => offset_ch1(7 downto 0) <= signed(data_in);
                when x"4013" => offset_ch1(15 downto 8)<= signed(data_in);

                -- CONFIGURAZIONE CH2 (0x4014 - 0x4017)
                when x"4014" => scale_ch2(7 downto 0)  <= signed(data_in);
                when x"4015" => scale_ch2(15 downto 8) <= signed(data_in);
                when x"4016" => offset_ch2(7 downto 0) <= signed(data_in);
                when x"4017" => offset_ch2(15 downto 8)<= signed(data_in);

                -- ACQUISIZIONE E CALCOLO (0x4018 - 0x401B)
                when x"4018" => 
                    adc_temp_l <= data_in; -- LSB CH1
                
                when x"4019" => -- MSB CH1 + TRIGGER CALCOLO CH1
                    adc_full_12bit <= signed(std_logic_vector'(x"0" & data_in(3 downto 0) & adc_temp_l));
                    v_adc  := signed(std_logic_vector'(x"0" & data_in(3 downto 0) & adc_temp_l)) - 2048;
                    v_mult := v_adc * scale_ch1;
                    y_result <= offset_ch1 - resize(v_mult(31 downto 16), 16);

                when x"401A" => 
                    adc_temp_l <= data_in; -- LSB CH2
                
                when x"401B" => -- MSB CH2 + TRIGGER CALCOLO CH2
                    v_adc  := signed(std_logic_vector'(x"0" & data_in(3 downto 0) & adc_temp_l)) - 2048;
                    v_mult := v_adc * scale_ch2;
                    y_result <= offset_ch2 - resize(v_mult(31 downto 16), 16);
					 when x"401E" => 
						  reg_trig_hyst <= resize(unsigned(data_in), 12); -- Carica l'isteresi da C
					 when x"4020" => reg_dds_inc(7 downto 0)   <= unsigned(data_in);
					when x"4021" => reg_dds_inc(15 downto 8)  <= unsigned(data_in);
					when x"4022" => reg_dds_inc(23 downto 16) <= unsigned(data_in);
					when x"4023" => reg_dds_inc(31 downto 24) <= unsigned(data_in);
                when others => null;
            end case;
        end if;
    end if;
end process;

    ------------------------------------------------------------------
    -- Read index calculation
    -- Calcolo indice di lettura RAM con base stabile
    ------------------------------------------------------------------
	 
process(rd_base_stable, view_offset, reg_index_int)
    variable v_addr : integer;
begin
    -- 1. Calcoliamo la somma usando gli interi (gestisce il PAN negativo)
    v_addr := to_integer(rd_base_stable) + 
              to_integer(view_offset) + 
              to_integer(reg_index_int);

    -- 2. Applichiamo il modulo 4096 per il buffer circolare.
    -- In VHDL il modulo su numeri negativi può essere insidioso, 
    -- quindi usiamo un trucco standard per riportarlo sempre positivo.
    v_addr := v_addr mod BUFFER_SIZE;
    if v_addr < 0 then
        v_addr := v_addr + BUFFER_SIZE;
    end if;

    -- 3. Riconvertiamo nel puntatore a 12 bit per la RAM
    rd_index <= to_unsigned(v_addr, PTR_BITS);
end process;

    ------------------------------------------------------------------
    -- Trigger sample selection
    -- Selezione canale ADC per la comparazione di trigger
    ------------------------------------------------------------------
	with trig_chan_sel select
		trig_sample <= adc_a(11 downto 0) when "00", -- Rimosso lo shift (11 downto 2)
                   adc_b(11 downto 0) when "01",
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
	 

-- 1. PROCESSO CALCOLO FREQUENZA (Ottimizzato per 60MHz)
process(clk, rst_n)
begin
    if rst_n = '0' then
        gate_count <= (others => '0');
        accumulator <= (others => '0');
        freq_period_reg <= (others => '0');
    elsif rising_edge(clk) then
        -- 1. L'accumulatore deve sempre contare (non fermarlo mai!)
        accumulator <= accumulator + 1;

        -- 2. Logica di cattura
        if trig_hit_raw = '1' then
            if gate_count >= 63 then 
                freq_period_reg <= accumulator; -- Salva il tempo totale di 64 cicli
                accumulator <= (others => '0'); -- Reset sincronizzato dell'accumulatore
                gate_count <= (others => '0');
            else
                gate_count <= gate_count + 1;
            end if;
        end if;

        -- 3. Watchdog Migliorato (Timeout)
        -- Se l'accumulatore supera un tempo ragionevole (es. 0.5 secondi o fondo scala)
        -- significa che la frequenza è troppo bassa o il segnale è sparito.
        if accumulator = x"03938700" then -- Esempio: 60MHz * 1 secondo (o usa FFFFFFFF)
            gate_count <= (others => '0');
            freq_period_reg <= (others => '0');
            accumulator <= (others => '0'); -- IMPORTANTE: Resetta l'accumulatore per farlo ripartire
        end if;
    end if;
end process;


-- 2. PROCESSO GENERAZIONE TRIGGER HIT (Pulizia Glitch)
process(clk, rst_n)
    constant HYST : unsigned(11 downto 0) := to_unsigned(20, 12);
begin
    if rst_n = '0' then
        trig_hit_raw  <= '0';
        v_trig_armed  <= '0';
        trig_prev_val <= (others => '0');
    elsif rising_edge(clk) then
        trig_hit_raw <= '0'; -- Impulso pulito di 1 clock
        trig_prev_val <= unsigned(trig_sample_sync); -- Questo è il NOSTRO riferimento precedente

        if trig_enable = '1' then
            -- RISING EDGE
            if trig_edge = '0' then 
                if unsigned(trig_sample_sync) < (unsigned(trig_level) - reg_trig_hyst) then
                    v_trig_armed <= '1';
                elsif v_trig_armed = '1' and (trig_prev_val < unsigned(trig_level)) 
                                         and (unsigned(trig_sample_sync) >= unsigned(trig_level)) then
                    trig_hit_raw <= '1';
                    v_trig_armed <= '0';
                end if;
            -- FALLING EDGE
            else 
                if unsigned(trig_sample_sync) > (unsigned(trig_level) + reg_trig_hyst) then
                    v_trig_armed <= '1';
                elsif v_trig_armed = '1' and (trig_prev_val > unsigned(trig_level)) 
                                         and (unsigned(trig_sample_sync) <= unsigned(trig_level)) then
                    trig_hit_raw <= '1';
                    v_trig_armed <= '0';
                end if;
            end if;
        end if;
    end if;
end process;

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
        dds_acc <= (others => '0');
        tick_en <= '0';
    elsif rising_edge(clk) then
        tick_en <= '0';
        
        if reg_dds_inc = 0 then
            -- Se 0, campioniamo alla massima velocità (60MHz)
            tick_en <= '1';
        else
            -- Accumulatore DDS
            dds_acc <= dds_acc + reg_dds_inc;
            
            -- Genera tick sull'overflow dell'accumulatore
            if dds_acc < reg_dds_inc then 
                tick_en <= '1';
            end if;
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
		  
		  
process(mem_ramadr, ram_a_out, ram_b_out, bram_ce, y_result)
begin
    -- Reset di default per evitare latch indesiderati
    bram_data_to_avr <= (others => '0');
    
    if bram_ce = '1' then
        case mem_ramadr is
            -- Lettura BRAM (Originale)
            when x"4000" => bram_data_to_avr <= std_logic_vector(ram_a_out(7 downto 0));
            when x"4001" => bram_data_to_avr <= "0000" & std_logic_vector(ram_a_out(11 downto 8));
            when x"4002" => bram_data_to_avr <= std_logic_vector(ram_b_out(7 downto 0));
            when x"4003" => bram_data_to_avr <= "0000" & std_logic_vector(ram_b_out(11 downto 8));
            
            -- Lettura Y_RESULT (16 bit divisi in due letture)
            when x"401C" => bram_data_to_avr <= std_logic_vector(y_result(7 downto 0));
            when x"401D" => bram_data_to_avr <= std_logic_vector(y_result(15 downto 8));
            
            when others => null;
        end case;
    end if;
end process;
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
        last_ramre       <= '1';
        ctrl_reset_index <= '0';

    elsif rising_edge(clk) then
        -- Aggiornamento segnale per rilevamento fronti
        last_ramre <= ramre;

        -- Gestione Timeout MMIO
        if mmio_we = '1' then
            wr_timeout <= to_unsigned(20000, wr_timeout'length);
        elsif wr_timeout /= 0 then
            wr_timeout <= wr_timeout - 1;
        else
            trig_bytecnt <= (others => '0');
            base_bytecnt <= (others => '0');
            view_bytecnt <= (others => '0'); 
        end if;
        
        -- Reset dell'indice via Software (Comando MMIO)
        if mmio_we = '1' and rst_cmd_sel = '1' then
            ctrl_reset_index <= mmio_wdata(0);
            reg_index_int    <= (others => '0'); -- Reset immediato anche dell'indice
        else
            ctrl_reset_index <= '0';
        end if;
          
        -- LOGICA AVANZAMENTO INDICE BRAM (Ottimizzata per 60MHz)
        if index_reg_sel = '1' and mmio_we = '1' then
             -- Scrittura manuale dall'AVR (es. per forzare un valore)
             reg_index_int <= "00" & unsigned(mmio_wdata);
             
        elsif ramre = '1' and last_ramre = '0' then
             -- Incremento alla FINE della lettura (Fronte di salita di ramre)
             -- Solo se siamo sull'ultimo byte del pacchetto (CH2_High -> "11")
             if bram_ce = '1' and mem_ramadr(3 downto 0) = "0011" then
                  if reg_index_int = 399 then 
                      reg_index_int <= (others => '0');
                  else
                      reg_index_int <= reg_index_int + 1;
                  end if;
             end if;
        end if;

        -- Gestione Timebase e Comandi Complessi (Escape xFF)
        if mmio_we = '1' and base_reg_sel = '1' then
            if view_bytecnt = "100" then
                view_full_sign <= signed(view_full_raw);
                view_offset    <= resize(signed(view_full_raw), view_offset'length);
                view_bytecnt   <= "000";
            elsif view_bytecnt = "010" then
                view_full_raw(7 downto 0) <= mmio_wdata;
                view_bytecnt <= "011";
            elsif view_bytecnt = "011" then
                view_full_raw(15 downto 8) <= mmio_wdata;
                view_bytecnt <= "100";
            elsif mmio_wdata = x"FF" then
                view_bytecnt <= "001";
            elsif view_bytecnt = "001" then
                base_cmd     <= mmio_wdata(0);
                view_bytecnt <= "010";
            else
                reg_time_div_sel <= to_integer(unsigned(mmio_wdata(4 downto 0)));
            end if;
        end if;

        -- Gestione Trigger Level (Multibyte)
        if mmio_we = '1' and trig_reg_sel = '1' then
            if trig_bytecnt = "00" then 
                trig_shift(7 downto 0) <= unsigned(mmio_wdata);
            elsif trig_bytecnt = "01" then 
                trig_shift(15 downto 8) <= unsigned(mmio_wdata);
            elsif trig_bytecnt = "10" then
                reg_trig_level <= trig_shift(11 downto 0); 
            end if;

            if trig_bytecnt = "10" then 
                trig_bytecnt <= (others => '0');
            else 
                trig_bytecnt <= trig_bytecnt + 1; 
            end if;
        end if;

        -- Trigger Control (Slope, Source, Mode)
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
                when REG_INDEX =>
                    mmio_rdata <= std_logic_vector(reg_index_int(7 downto 0));
                    out_en <= '1';
                when REG_TRIG =>
                    mmio_rdata <= reg_trig_ctrl or ("000000" & ready & '0');
                    out_en <= '1';
					 when REG_FREQ0 => 
                    mmio_rdata <= std_logic_vector(freq_period_latch(7 downto 0));
						  out_en <= '1';
                when REG_FREQ1 => 
                    mmio_rdata <= std_logic_vector(freq_period_latch(15 downto 8));
						  out_en <= '1';
                when REG_FREQ2 => 
                    mmio_rdata <= std_logic_vector(freq_period_latch(23 downto 16));
						  out_en <= '1';
                when REG_FREQ3 => 
                    mmio_rdata <= std_logic_vector(freq_period_latch(31 downto 24));
						  out_en <= '1';
                when others =>
                    out_en <= '0';
            end case;
        end if;
    end process;
	 
	process(clk, rst_n)
	begin
		 if rst_n = '0' then
			  freq_period_latch <= (others => '0');
		 elsif rising_edge(clk) then
			  -- Se l'AVR sta leggendo il primo registro della frequenza
			  if iore = '1' and mmio_addr = REG_FREQ0 then
					freq_period_latch <= freq_period_reg;
			  end if;
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
                rd_base_latched <= trig_wr_ptr - to_unsigned(VIEW_BEGIN, PTR_BITS);
            end if;
        end if;
    end process;

    ------------------------------------------------------------------
    -- ADC reader instantiation
    ------------------------------------------------------------------
    adc_reader_inst : entity work.dual_ad9226_reader
        port map(
					clk65   => clk,   
					rst_n => rst_n,
						-- Interfaccia Modulo ADC Canale A
				  DATA_A   => DATA_A, -- A1..12
				  ACK      => ACK,                     -- Clock A
				  ORA      => ORA,                     -- Out of Range A
				  
				  -- Interfaccia Modulo ADC Canale B
				  DATA_B  => DATA_B, -- B1..12
				  BCK     => BCK,                     -- Clock B
				  ORB     => ORB,                     -- Out of Range B
						
				
              ch_a_val   => adc_a, 
				  ch_b_val   => adc_b 
        );
		  


end architecture;