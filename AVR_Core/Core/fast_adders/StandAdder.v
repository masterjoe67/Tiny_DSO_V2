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
//-- Module:          Adder
//----------------------------------------------------------------------------------

`timescale 1 ns / 1 ps


module StandAdder(A, B, CI, S, CO);
   parameter               AdderWidth = 16;
   input [AdderWidth-1:0]  A;
   input [AdderWidth-1:0]  B;
   input                   CI;
   output [AdderWidth-1:0] S;
   output                  CO;
   
   wire [AdderWidth-1+1:0] TmpRes;
   
   assign TmpRes = (({1'b0, A}) + ({1'b0, B})) + CI;
   assign S = TmpRes[AdderWidth-1:0];
   assign CO = TmpRes[AdderWidth-1+1];
   
endmodule
