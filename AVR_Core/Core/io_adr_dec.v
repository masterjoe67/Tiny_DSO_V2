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
//-- Module:          Internal I/O registers decoder/multiplexer for the AVR core
//----------------------------------------------------------------------------------


`timescale 1 ns / 1 ns

module io_adr_dec #(
                    parameter pc22b = 0
		    ) 
		    (
   		     input wire [6:0]  adr,
   		     input wire        iore,
   		     input wire [7:0]  dbusin_ext,
   		     output reg [7:0]  dbusin_int,
   		     // SREG/SPL/SPH/RAMPZ i/f  	
   		     input wire [7:0]  spl_out,
   		     input wire [7:0]  sph_out,
   		     input wire [7:0]  sreg_out,
   		     input wire [7:0]  rampz_out,
   		     input wire [7:0]  eind_out
                     );

//************************************************************************************************

// Register addresses  
localparam P_SPL_Address   = 6'h3D; // Stack Pointer(Low)
localparam P_SPH_Address   = 6'h3E; // Stack Pointer(High)
localparam P_SREG_Address  = 6'h3F; // Status Register
localparam P_RAMPZ_Address = 6'h3B; // RAM Page Z Select Register
localparam P_EIND_Address  = 6'h3C; // !!!TBD!!! Occupated by XDIV in Mega128
  
//************************************************************************************************  
   
   generate
      if (!pc22b)
      begin : no_eind
         
         always @*
         begin: out_mux_comb
            if (iore)
               case (adr)
                  P_SPL_Address   : dbusin_int = spl_out;
                  P_SPH_Address   : dbusin_int = sph_out;
                  P_SREG_Address  : dbusin_int = sreg_out;
                  P_RAMPZ_Address : dbusin_int = rampz_out;
                  default         : dbusin_int = dbusin_ext;
               endcase
            else
               dbusin_int = dbusin_ext;
            end // out_mux_comb
         end // no_eind

         else // (pc22b != 0)
         begin : eind_is_impl
            
            always @*
            begin: out_mux_comb
               if (iore)
                  case (adr)
                     P_SPL_Address   : dbusin_int = spl_out;
                     P_SPH_Address   : dbusin_int = sph_out;
                     P_SREG_Address  : dbusin_int = sreg_out;
                     P_RAMPZ_Address : dbusin_int = rampz_out;
                     P_EIND_Address  : dbusin_int = eind_out;
                     default         : dbusin_int = dbusin_ext;
                  endcase
               else
                  dbusin_int = dbusin_ext;
               end // out_mux_comb
            end // eind_is_impl
         endgenerate
         
endmodule // io_adr_dec

