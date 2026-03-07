--************************************************************************************************
-- ADC reader module AD9226
-- Version 1.0 (Version for Intel)
-- Designed by Giovanni Legati
-- M.J.E. 2026
-- masterjoe67@hotmail.it
--************************************************************************************************
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity dual_ad9226_reader is
    port(
        clk65       : in  std_logic;    -- Clock di campionamento (max 65 MHz)
        rst_n       : in  std_logic;

        -- Interfaccia Modulo ADC Canale A
        DATA_A           : in  std_logic_vector(11 downto 0); -- A1..12
        ACK         : out std_logic;                     -- Clock A
        ORA         : in  std_logic;                     -- Out of Range A
        
        -- Interfaccia Modulo ADC Canale B
        DATA_B           : in  std_logic_vector(11 downto 0); -- B1..12
        BCK         : out std_logic;                     -- Clock B
        ORB         : in  std_logic;                     -- Out of Range B

        -- Registri in uscita 
        ch_a_val    : out unsigned(11 downto 0);
        ch_b_val    : out unsigned(11 downto 0);
        overrange   : out std_logic_vector(1 downto 0)   -- [1]=ORB, [0]=ORA
    );
end entity;

architecture rtl of dual_ad9226_reader is
    signal reg_a : unsigned(11 downto 0) := (others => '0');
    signal reg_b : unsigned(11 downto 0) := (others => '0');
    signal reg_ora, reg_orb : std_logic := '0';
begin

    -- Pilotaggio clock ADC
    ACK <= clk65;
    BCK <= clk65;

    -- Assegnazione output
    ch_a_val  <= reg_a;
    ch_b_val  <= reg_b;
    overrange <= reg_orb & reg_ora;

    ------------------------------------------------------------------
    -- Campionamento Sincrono
    ------------------------------------------------------------------
    process(clk65, rst_n)
    begin
        if rst_n = '0' then
            reg_a   <= (others => '0');
            reg_b   <= (others => '0');
            reg_ora <= '0';
            reg_orb <= '0';
        elsif falling_edge(clk65) then
            -- Acquisizione dati e stati di errore
            reg_a   <= unsigned(DATA_A);
            reg_b   <= unsigned(DATA_B);
            reg_ora <= ORA;
            reg_orb <= ORB;
        end if;
    end process;

end architecture;