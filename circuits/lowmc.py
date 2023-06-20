import random
import sys

# LowMC params
blocksize = 128
nrounds = 9
nsboxes = 42
m4r_window_size = 4

num_wire_used = 0
gates = []
input_sizes = []
output_sizes = []


def make_block(width):
    global num_wire_used
    num_wire_used += width
    return (num_wire_used - width, num_wire_used)

def make_input_block(width):
    input_sizes.append(width)
    return make_block(width)

def make_output_block(width):
    output_sizes.append(width)
    return make_block(width)

def width_of(b):
    return b[1] - b[0]

def first_wire(b):
    return b[0]

def one_wire_block(w):
    return (w, w+1)

def xor_wires(w1, w2, *, output):
    assert(w1 is not None)
    assert(w2 is not None)
    assert(output is not None)
    gates.append([2,1,w1,w2,output, "XOR"])
    return output

def inv_wires(w1, *, output):
    gates.append([1,1,w1,output,"INV"])
    return output
    
def xor_gate(inputs_1, inputs_2, *, outputs):
    b1_start = first_wire(inputs_1)
    b2_start = first_wire(inputs_2)
    output_start = first_wire(outputs)
    width = width_of(inputs_1)
    assert width_of(inputs_2) == width
    assert width_of(outputs) == width
    for i in range(width):
        xor_wires(b1_start+i, b2_start+i, output=output_start+i)
    return outputs

def sub_block(b, *, sub_block_size, sub_block_index):
    return (first_wire(b) + sub_block_index * sub_block_size, 
            first_wire(b) + (sub_block_index + 1) * sub_block_size)

def list_wires(b):
    return list(range(*b))

def range_wires(b):
    return range(*b)

def put_custom_mand(*, inputs_1, inputs_2, outputs):
    assert len(inputs_1) == len(inputs_2)
    assert len(inputs_1) == len(outputs)
    width = len(inputs_1)
    gates.append([2 * width, width] + inputs_1 + inputs_2 + outputs + ["MAND"])


# enforcing statesize = blocksize
def BuildLowMCCircuit():
    # let's not overwrite input for now, can consider later
    expanded_key = make_input_block((nrounds + 1) * blocksize)
    input_block = make_input_block(blocksize)
    zero_block = make_block(blocksize)
    zero_wire = first_wire(zero_block)
    one_wire = inv_wires(zero_wire, output=first_wire(make_block(1)))
    state = xor_gate(input_block, zero_block, outputs=make_block(blocksize))
    tmp_state = make_block(blocksize)
    m4r_lut = make_block(1 << m4r_window_size)

    LowMCAddRoundKey(state=state, 
                     round_key=sub_block(expanded_key, sub_block_size=blocksize, sub_block_index=0))

    for round in range(nrounds):
        print(f"round {round}", file=sys.stderr)
        LowMCPutSBoxLayer(input=state,
                          output=tmp_state,
                          zero_wire=zero_wire)
        FourRussiansMatrixMult(input=tmp_state, lut=m4r_lut, output=state)
        LowMCXORConstants(state, one_wire)
        LowMCAddRoundKey(state=state, 
                         round_key=sub_block(expanded_key, sub_block_size=blocksize, 
                                             sub_block_index=(round+1)))
                                        
    output_block = make_output_block(blocksize)
    xor_gate(zero_block, state, outputs=output_block)


def LowMCAddRoundKey(*, state, round_key):
    xor_gate(state, round_key, outputs=state)

def LowMCXORConstants(state, one_wire):
    for i in range(blocksize):
        if random.randint(0,1):
            xor_wires(first_wire(state) + i, one_wire, 
                     output=first_wire(state) + i)

def LowMCPutSBoxLayer(*, input, output, zero_wire):
    # first do all ands
    # then mop up with xors
    # result stored in tmp_state
    assert(width_of(input) == blocksize)
    assert(width_of(output) == blocksize)

    input_start = first_wire(input)
    output_start = first_wire(output)
    mand_inputs_1 = []
    mand_inputs_2 = []
    mand_outputs = list_wires(output)[:3 * nsboxes]

    for i in range(nsboxes):
        a = input_start + 3*i + 0
        b = input_start + 3*i + 1
        c = input_start + 3*i + 2

        d = output_start + 3*i + 0
        e = output_start + 3*i + 1
        f = output_start + 3*i + 2

        # BC + A
        mand_inputs_1.append(b)
        mand_inputs_2.append(c)
        # CA + A + B
        mand_inputs_1.append(c)
        mand_inputs_2.append(a)
        # AB + A + B + C
        mand_inputs_1.append(a)
        mand_inputs_2.append(b)
 
    put_custom_mand(inputs_1=mand_inputs_1, inputs_2=mand_inputs_2,
                    outputs=mand_outputs)

    for i in range(nsboxes):
        a = input_start + 3*i + 0
        b = input_start + 3*i + 1
        c = input_start + 3*i + 2

        d = output_start + 3*i + 0
        e = output_start + 3*i + 1
        f = output_start + 3*i + 2

        # BC + A
        xor_wires(d,a,output=d)
        # CA + A + B
        xor_wires(e,a,output=e)
        xor_wires(e,b,output=e)
        # AB + A + B + C
        xor_wires(f,a,output=f)
        xor_wires(f,b,output=f)
        xor_wires(f,c,output=f)

    for i in range(3*nsboxes, blocksize):
        xor_wires(input_start+i, zero_wire, output=output_start+i)

def ctz(n):
    return (n ^ (n - 1)).bit_length() - 1

# in gray code order 
def fill_out_lut(*, lut, input):
    assert(width_of(lut) == (1 << m4r_window_size))
    assert(width_of(input) == m4r_window_size)
    # assumed to also be a zero wire
    lut_start = first_wire(lut)
    input_start = first_wire(input)
    # quick sanity check
    masks_covered = [0]
    for i in range(1, 1 << m4r_window_size):
        xor_wires(lut_start + i - 1, input_start + ctz(i), output = lut_start + i)
        masks_covered.append(masks_covered[-1] ^ (1 << ctz(i)))
    assert(sorted(masks_covered) == list(range(1 << m4r_window_size)))

 
def FourRussiansMatrixMult(*, input, lut, output):
    assert(width_of(input) == blocksize)
    assert(width_of(output) == blocksize)
    # remove if/when I implement remainder lut
    assert(blocksize % m4r_window_size == 0)

    for i in range(blocksize // m4r_window_size):
        print(i, file=sys.stderr)
        fill_out_lut(lut=lut, input=sub_block(input, 
                                              sub_block_size=m4r_window_size, 
                                              sub_block_index=i))
        for output_wire in range_wires(output):
            # first_wire(lut) assumed to be zero always
            xor_wires(first_wire(lut) if i == 0 else output_wire, 
                      first_wire(lut) + random.randint(0, (1 << m4r_window_size) - 1), 
                      output=output_wire)


def test_eval_circuit(wires):
    for gate in gates:
        if gate[-1] == "XOR":
            wires[gate[4]] = wires[gate[2]] ^ wires[gate[3]]
        elif gate[-1] == "MAND":
            num_ands_in_mand = gate[1]
            for i in range(num_ands_in_mand):
                wires[gate[2+2*num_ands_in_mand+i]] = wires[gate[2+i]] & wires[gate[2+num_ands_in_mand+i]]
        elif gate[-1] == "INV":
            wires[gate[3]] = not wires[gate[2]]
                
    
    

if __name__ == "__main__" :
    BuildLowMCCircuit()
    print(len(gates), num_wire_used)
    print(len(input_sizes), " ".join(map(str,input_sizes)))
    print(len(output_sizes), " ".join(map(str,output_sizes)))
    print()
    for gate in gates:
        print(" ".join(map(str, gate)))
    wires = [0] * num_wire_used
    for i in range(sum(input_sizes)):
        wires[i] = random.randint(0,1)
    test_eval_circuit(wires)
    print(wires[-128:], sum(wires[-128:]), file=sys.stderr)
    