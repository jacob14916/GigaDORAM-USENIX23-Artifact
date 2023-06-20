"""
Bristol Fashion:
Gates Wires
Inputs Bits_1 ... Bits_Inputs
Outputs Bits_1 ...

Inputs Outputs Wire1 Wire2 [Wire3] Type
...

circuit input format (4 blocks)
key | lookup_value_0 | lookup_value_1 | (dummy_index | 96b unused)

output format (1 block)
(index | found | 88b unspecified)
"""

num_wire_used = 0
gates = []

def make_block(width):
    global num_wire_used
    num_wire_used += width
    return (num_wire_used - width, num_wire_used)

def width_of(b):
    return b[1] - b[0]

def one_input_gate(b1, gate_type):
    assert gate_type in ("INV",)
    b1_start, b1_end = b1
    width = width_of(b1)
    b2 = make_block(width)
    b2_start, b2_end = b2
    for i in range(width):
        gates.append([1,1,b1_start+i, b2_start+i, gate_type])
    return b2
       
def two_input_gate(b1, b2, gate_type):
    assert gate_type in ("AND", "XOR")
    b1_start, b1_end = b1
    b2_start, b2_end = b2
    width = width_of(b1)
    assert width_of(b2) == width
    b3 = make_block(width)
    b3_start, b3_end = b3
    for i in range(width):
        gates.append([2,1,b1_start+i, b2_start+i, b3_start+i, gate_type])
    return b3


def inv_gate(b1):
    return one_input_gate(b1, "INV")

def and_gate(b1, b2):
    return two_input_gate(b1, b2, "AND")

def xor_gate(b1, b2):
    return two_input_gate(b1, b2, "XOR")

def and_all(b):
    start, end = b
    width = width_of(b)
    if width == 1:
        return b
    mid = (start + end) // 2 
    left = and_all((start, mid))
    right = and_all((mid, end))
    return and_gate(left, right)

def if_then_else(cond, then_, else_):
    assert width_of(cond) == 1
    assert width_of(then_) == width_of(else_)
    cond_start, _ = cond
    width = width_of(then_)
    then_xor_else_start, _ = xor_gate(then_, else_)
    product = make_block(width)
    product_start, _ = product
    for i in range(width):
        gates.append([2,1,cond_start,then_xor_else_start+i,product_start+i,"AND"])
    return xor_gate(product, else_) 

# by using variables, topsort order is enforced!        
key_unused = make_block(32)
key_tag = make_block(96)
cht_b0_index = make_block(32)
cht_b0_tag = make_block(96)
cht_b1_index = make_block(32)
cht_b1_tag = make_block(96)
dummy_index = make_block(32)
dummy_unused = make_block(96)

key_xor_0_tag = xor_gate(key_tag, cht_b0_tag)
key_xor_1_tag = xor_gate(key_tag, cht_b1_tag)
key_nxor_0_tag = inv_gate(key_xor_0_tag)
key_nxor_1_tag = inv_gate(key_xor_1_tag)
key_equals_b0 = and_all(key_nxor_0_tag)
key_equals_b1 = and_all(key_nxor_1_tag)
# begin output section
output_index = if_then_else(key_equals_b0, cht_b0_index, if_then_else(key_equals_b1, cht_b1_index, dummy_index))
# using that xor_gate doesn't allocate any additional wires
output_found = xor_gate(key_equals_b0, key_equals_b1)
output_unused = make_block(95)

print(len(gates), num_wire_used)
# splitting inputs emphasizes little endian
print(4, 128, 128, 128, 128)
print(1, 128)
print("")
for gate in gates:
    print(" ".join(map(str, gate)))
