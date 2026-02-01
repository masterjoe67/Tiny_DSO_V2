--************************************************************************************************
-- ADC reader module 
-- Version 0.5 (Version for Intel)
-- Designed by Giovanni Legati
-- M.J.E. 2026
-- masterjoe67@hotmail.it
--************************************************************************************************

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity adc128s022_reader is
    port(
        clk     : in  std_logic;   -- 40 MHz
        rst_n   : in  std_logic;

        -- Linee SPI verso l'ADC
        sclk    : out std_logic;
        cs_n    : out std_logic;
        mosi    : out std_logic;
        miso    : in  std_logic;

        -- Registri canali in uscita
        ch0     : out unsigned(11 downto 0);
        ch1     : out unsigned(11 downto 0);
        ch2     : out unsigned(11 downto 0)
    );
end entity;

architecture rtl of adc128s022_reader is

    ------------------------------------------------------------------
    -- Segnali di Stato e Registri Interni
    ------------------------------------------------------------------
    type ADC_STATE is (ADC_SEL, TRIGGER, ACQUIRE, STORE);
    signal STATE : ADC_STATE := ADC_SEL;
    
    signal oCLK   : STD_LOGIC := '0'; -- Clock interno per la gestione SPI
    signal CS_BUF : STD_LOGIC := '1'; -- Buffer per il Chip Select attivo basso
    
    signal reg_ch0, reg_ch1, reg_ch2 : unsigned(11 downto 0) := (others => '0');
    
    -- Pipeline a due stadi per la gestione della latenza dei canali
    signal ch_req_d1 : integer range 0 to 3 := 0; -- Canale chiesto ora
    signal ch_req_d2 : integer range 0 to 3 := 0; -- Canale chiesto nel ciclo precedente

begin

    ------------------------------------------------------------------
    -- Assegnazione Output Segnali
    ------------------------------------------------------------------
    cs_n <= CS_BUF;
    ch0  <= reg_ch0;
    ch1  <= reg_ch1;
    ch2  <= reg_ch2;

    -- SCLK abilitato solo durante la fase di ACQUIRE
    sclk <= not oCLK when STATE = ACQUIRE else '1';

    ------------------------------------------------------------------
    -- Generazione Clock ADC (Divisore 40MHz / 10 = 4MHz)
    ------------------------------------------------------------------
    process(clk, rst_n)
        variable count : integer range 0 to 6 := 0;
    begin
        if rst_n = '0' then
            count := 0; 
            oCLK  <= '0';
        elsif rising_edge(clk) then
            if count = 6 then
                oCLK  <= not oCLK; 
                count := 0;
            else
                count := count + 1;
            end if;
        end if;
    end process;

    ------------------------------------------------------------------
    -- Macchina a Stati Finita (FSM) per Protocollo SPI
    ------------------------------------------------------------------
    process(oCLK, rst_n)
        variable DATA_COUNT   : integer range 0 to 15 := 0;
        variable ADC_CH_INDEX : integer range 0 to 4  := 0; -- Contatore ciclico 0,1,2,3
        variable ADC_DATA     : std_logic_vector(11 downto 0);
        variable iCH          : std_logic_vector(3 downto 0);
    begin
        if rst_n = '0' then
            STATE        <= ADC_SEL; 
            CS_BUF       <= '1';
            ADC_CH_INDEX := 0;
            ch_req_d1    <= 0;
            ch_req_d2    <= 0;
        elsif rising_edge(oCLK) then
            case STATE is

                -- Fase 1: Selezione Indirizzo Canale
                when ADC_SEL =>
                    -- Selezioniamo il canale (0, 1, 2 o 3 fantasma)
                    iCH   := std_logic_vector(to_unsigned(ADC_CH_INDEX, 4));
                    STATE <= TRIGGER;

                -- Fase 2: Attivazione Chip Select
                when TRIGGER =>
                    CS_BUF <= '0';
                    STATE  <= ACQUIRE;

                -- Fase 3: Scambio Dati (Shift Register)
                when ACQUIRE =>
                    case DATA_COUNT is
                        -- Invio indirizzo su MOSI (3 bit significativi)
                        when 1  => mosi <= iCH(2);
                        when 2  => mosi <= iCH(1);
                        when 3  => mosi <= iCH(0);
                        -- Ricezione campionamento su MISO (12 bit)
                        when 4  => ADC_DATA(11) := miso;
                        when 5  => ADC_DATA(10) := miso;
                        when 6  => ADC_DATA(9)  := miso;
                        when 7  => ADC_DATA(8)  := miso;
                        when 8  => ADC_DATA(7)  := miso;
                        when 9  => ADC_DATA(6)  := miso;
                        when 10 => ADC_DATA(5)  := miso;
                        when 11 => ADC_DATA(4)  := miso;
                        when 12 => ADC_DATA(3)  := miso;
                        when 13 => ADC_DATA(2)  := miso;
                        when 14 => ADC_DATA(1)  := miso;
                        when 15 => ADC_DATA(0)  := miso;
                        when others => null;
                    end case;

                    -- Gestione contatore bit
                    if DATA_COUNT = 15 then
                        DATA_COUNT := 0; 
                        STATE      <= STORE;
                    else
                        DATA_COUNT := DATA_COUNT + 1;
                    end if;

                -- Fase 4: Chiusura Transazione e Salvataggio Registri
                when STORE =>
                    CS_BUF <= '1';
                    
                    -- Salvataggio dato ricevuto (allineato a ch_req_d2)
                    case ADC_CH_INDEX is
                        when 0 => reg_ch0 <= unsigned(ADC_DATA);
                        when 1 => reg_ch1 <= unsigned(ADC_DATA);
                        when 2 => reg_ch2 <= unsigned(ADC_DATA);
                        when others => null; -- Ignoriamo il canale 3
                    end case;

                    -- Incremento circolare su 4 canali (0,1,2,3)
                    if ADC_CH_INDEX = 4 then 
                        ADC_CH_INDEX := 0;
                    else 
                        ADC_CH_INDEX := ADC_CH_INDEX + 1;
                    end if;

                    STATE <= ADC_SEL;
                    
            end case;
        end if;
    end process;

end architecture;