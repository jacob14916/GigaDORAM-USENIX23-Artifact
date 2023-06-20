// this was an attempt at using verilog for circuits, which failed because
// convert_yosys.py had serious problems
// but led to a better handmade python circuit generator
module cht_lookup (input wire [511:0] input_, 
                    output wire [127:0] output_);
// currently there are some wasted wires because my GC code assumes
// that the circuit operates on blocks
// there is no great reason for that assumption, and it will hopefully be removed
wire [127:0] key;
wire [127:0] b0;
wire [127:0] b1;
wire [31:0] dummy;

assign key = input_[127:0];
assign b0 = input_[255:128];
assign b1 = input_[383:256];
assign dummy = input_[415:384];

wire key_eq_b0;
wire key_eq_b1;

assign key_eq_b0 = (key[127:32] == b0[127:32]);
assign key_eq_b1 = (key[127:32] == b1[127:32]);
assign output_[31:0] = key_eq_b0 ? b0[31:0] : (key_eq_b1 ? b1[31:0] : dummy);

endmodule
