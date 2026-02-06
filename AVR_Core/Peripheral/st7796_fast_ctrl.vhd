library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.AVRuCPackage.all;

entity st7796_fast_ctrl is
    port (
        clk         : in  std_logic;    -- 50MHz
        clk_spi     : in  std_logic;    -- 20MHz
        rst_n       : in  std_logic;
        adr         : in  std_logic_vector(IOAdrWidth-1 downto 0);
        dbus_in     : in  std_logic_vector(7 downto 0);
        dbus_out    : out std_logic_vector(7 downto 0);
        iore        : in  std_logic;
        iowe        : in  std_logic;
        out_en      : out std_logic;
        tft_sclk    : out std_logic;
        tft_mosi    : out std_logic;
        tft_cs      : out std_logic;
        tft_dc      : out std_logic;
        tft_rst     : out std_logic
    );
end entity;

architecture rtl of st7796_fast_ctrl is
    type fifo_array is array (0 to 15) of std_logic_vector(8 downto 0);
    signal fifo_mem : fifo_array;
    signal wr_ptr, rd_ptr : unsigned(3 downto 0) := (others => '0');

    -- Sincronizzatori
    signal rd_ptr_s1, rd_ptr_s2 : unsigned(3 downto 0) := (others => '0');
    signal wr_ptr_s1, wr_ptr_s2 : unsigned(3 downto 0) := (others => '0');
    signal running_s1, running_s2 : std_logic := '0';

    signal dc_latch : std_logic := '0';
    signal rst_reg  : std_logic := '0';
    signal running  : std_logic := '0';
    signal bit_cnt  : integer range 0 to 9 := 0;
    signal shift_reg: std_logic_vector(7 downto 0);
    signal dc_out   : std_logic := '0';
    signal busy_reg : std_logic := '0';

begin
    -- Segnali SPI diretti
    tft_rst <= rst_reg;
    tft_dc  <= dc_out;
    tft_cs  <= '0' when (running = '1' or rd_ptr /= wr_ptr_s2) else '1';
    --tft_sclk <= clk_spi when (running = '1' and bit_cnt > 0) else '0';
	 --tft_sclk <= clk_spi when (running = '1' and bit_cnt < 9 and bit_cnt > 0) else '0';
	 tft_sclk <= (clk_spi) when (running = '1' and bit_cnt > 0 and bit_cnt < 9) else '0';
	 

    -- 1. DOMINIO CPU (50MHz) - Scrittura e Calcolo Busy
    process(clk, rst_n)
        variable diff : unsigned(3 downto 0);
    begin
        if rst_n = '0' then
            wr_ptr <= (others => '0');
            rd_ptr_s1 <= (others => '0'); rd_ptr_s2 <= (others => '0');
            running_s1 <= '0'; running_s2 <= '0';
            dc_latch <= '0'; rst_reg <= '0';
            busy_reg <= '0';
        elsif rising_edge(clk) then
            -- Sincronizzazione segnali da SPI a CPU
            rd_ptr_s1 <= rd_ptr;
            rd_ptr_s2 <= rd_ptr_s1;
            running_s1 <= running;
            running_s2 <= running_s1;

            -- Calcolo Busy
            diff := wr_ptr - rd_ptr_s2;
            if (diff > 0) or (running_s2 = '1') then
                busy_reg <= '1';
            else
                busy_reg <= '0';
            end if;

            -- Scrittura registri
            if iowe = '1' then
                if adr = LCD_CTRL_REG then
                    dc_latch <= dbus_in(0);
                    rst_reg  <= dbus_in(1);
                elsif adr = LCD_DATA_REG then
                    fifo_mem(to_integer(wr_ptr)) <= dc_latch & dbus_in;
                    wr_ptr <= wr_ptr + 1;
                end if;
            end if;
        end if;
    end process;

    -- 2. DOMINIO SPI (20MHz) - Trasmissione
--    process(clk_spi, rst_n)
--    begin
--        if rst_n = '0' then
--            rd_ptr <= (others => '0');
--            wr_ptr_s1 <= (others => '0'); wr_ptr_s2 <= (others => '0');
--            running <= '0'; bit_cnt <= 0; dc_out <= '0'; tft_mosi <= '0';
--        elsif rising_edge(clk_spi) then
--            wr_ptr_s1 <= wr_ptr;
--            wr_ptr_s2 <= wr_ptr_s1;
--
--            if running = '0' then
--                if rd_ptr /= wr_ptr_s2 then
--                    dc_out    <= fifo_mem(to_integer(rd_ptr))(8);
--                    shift_reg <= fifo_mem(to_integer(rd_ptr))(7 downto 0);
--                    rd_ptr    <= rd_ptr + 1;
--                    bit_cnt   <= 8;
--                    running   <= '1';
--                end if;
--            else
--                if bit_cnt > 0 then
--                    tft_mosi <= shift_reg(7);
--                    shift_reg <= shift_reg(6 downto 0) & '0';
--                    bit_cnt <= bit_cnt - 1;
--                else
--                    running <= '0';
--                end if;
--            end if;
--        end if;
--    end process;

-- 2. DOMINIO SPI (20MHz)
process(clk_spi, rst_n)
    begin
        if rst_n = '0' then
            rd_ptr <= (others => '0');
            wr_ptr_s1 <= (others => '0'); wr_ptr_s2 <= (others => '0');
            running <= '0'; 
            bit_cnt <= 0; 
            dc_out <= '0'; 
            tft_mosi <= '0';
        elsif rising_edge(clk_spi) then
            wr_ptr_s1 <= wr_ptr;
            wr_ptr_s2 <= wr_ptr_s1;

            if running = '0' then
                if rd_ptr /= wr_ptr_s2 then
                    -- FASE DI CARICAMENTO
                    dc_out    <= fifo_mem(to_integer(rd_ptr))(8);
                    shift_reg <= fifo_mem(to_integer(rd_ptr))(7 downto 0);
                    rd_ptr    <= rd_ptr + 1;
                    
                    running   <= '1';
                    bit_cnt   <= 9; -- Impostiamo a 9 per creare il ritardo di setup
                end if;
            else
                if bit_cnt = 9 then
                    -- CICLO DI SETUP: Il DC è già uscito sopra, 
                    -- qui mettiamo solo il primo bit sul MOSI.
                    -- SCLK rimarrà a 0 grazie alla logica combinatoria sotto.
                    tft_mosi <= shift_reg(7);
                    bit_cnt <= 8;
                elsif bit_cnt > 0 then
                    -- TRASMISSIONE VERA: SCLK inizierà a pulsare qui
                    tft_mosi <= shift_reg(6); -- Prepara il bit dopo
                    shift_reg <= shift_reg(6 downto 0) & '0';
                    bit_cnt <= bit_cnt - 1;
                else
                    running <= '0';
                end if;
            end if;
        end if;
    end process;

    -- 3. INTERFACCIA BUS (Combinatoria pura fuori dai processi)
    -- Questa parte è critica: deve rispondere senza clock
    out_en <= '1' when iore = '1' and (adr = LCD_STATUS_REG or adr = LCD_CTRL_REG) else '0';
    
    dbus_out <= (busy_reg & "0000000") when adr = LCD_STATUS_REG else
                ("000000" & rst_reg & dc_latch) when adr = LCD_CTRL_REG else
                (others => '0');

end architecture;