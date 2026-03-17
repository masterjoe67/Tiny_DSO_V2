//----------------------------------------------------------------------------------
//-- Project:         Zoe DSO - AVR Soft Core
//-- Engineer:        Giovanni Legati M.J.E.
//-- 
//-- Description:     VHDL implementation of the AVR Soft Core architecture.
//--                  This module executes the main control logic for the Zoe 
//--                  oscilloscope, managing UI, triggers, and data processing.
//--
//-- Original Author: Ruslan Lepetenok
//-- Modified by:     Giovanni Legati M.J.E.
//--
//-- Revision:        16/03/2026
//--
//-- Module:          16-bit carry look-ahead adder for AVR (built using 8-bit CLA sections)
//----------------------------------------------------------------------------------

`timescale 1 ns / 1 ns


module CLA16B2x8S(A, B, CI, S, CO);
   input [15:0]  A;
   input [15:0]  B;
   input         CI;
   output [15:0] S;
   output        CO;
   
   wire          InternalCarry;
   
   
   CLA8B AdderB0to7_Inst(.a_in(A[7:0]), .b_in(B[7:0]), .c_in(CI), .s_out(S[7:0]), .c_out(InternalCarry));
   
   
   CLA8B AdderB8to15_Inst(.a_in(A[15:8]), .b_in(B[15:8]), .c_in(InternalCarry), .s_out(S[15:8]), .c_out(CO));
   
endmodule
