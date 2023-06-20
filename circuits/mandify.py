#!/usr/bin/env python3
num_gate, num_wire = map(int, input().split())
input_output_spec = input()
while (input() != ""):
    pass

# unify gate and output wire
wire_type = ["INPUT" for i in range(num_wire)]
wire_deps = [[] for i in range(num_wire)]
wire_children = [[] for i in range(num_wire)]
wire_num_deps_left = [0] * num_wire
gate_specs = [None for i in range(num_wire)]

for gate in range(num_gate):
    next_line = input().strip()
    if next_line == "" or next_line[0] == "#":
        # allow blank lines and comments
        continue
    gate_spec = next_line.split()
    num_in_wire, num_out_wire = map(int, gate_spec[0:2])
    assert(num_out_wire == 1)
    if num_in_wire == 1:
        in_wire = int(gate_spec[2])
        out_wire = int(gate_spec[3])
        assert(gate_spec[4] == "INV")
        wire_children[in_wire].append(out_wire)
        wire_deps[out_wire].append(in_wire)
        wire_num_deps_left[out_wire] = 1
        wire_type[out_wire] = gate_spec[4]
        gate_specs[out_wire] = gate_spec
    if num_in_wire == 2:
        in_wire1 = int(gate_spec[2])
        in_wire2 = int(gate_spec[3])
        out_wire = int(gate_spec[4])
        wire_children[in_wire1].append(out_wire)
        wire_children[in_wire2].append(out_wire)
        wire_deps[out_wire].append(in_wire1)
        wire_deps[out_wire].append(in_wire2)
        wire_num_deps_left[out_wire] = 2
        wire_type[out_wire] = gate_spec[5]
        gate_specs[out_wire] = gate_spec

# greedy algo:
# topsort where we put off ANDs in the queue until there are no XORs left
# then do all the ANDS in a batch
and_queue = []
all_other_queue = []
new_gates = []

for i in range(num_wire):
    if wire_num_deps_left[i] == 0:
        assert(wire_type[i] == "INPUT")
        all_other_queue.append(i)

def pluck(curr):
    assert(wire_num_deps_left[curr] == 0)
    for child in wire_children[curr]:
        wire_num_deps_left[child] -= 1
        if wire_num_deps_left[child] == 0:
            if wire_type[child] == "AND":
                and_queue.append(child)
            else:
                all_other_queue.append(child)
        
while (len(and_queue) + len(all_other_queue)):
    if len(all_other_queue):
        curr = all_other_queue.pop()
        if wire_type[curr] != "INPUT":
            new_gates.append(gate_specs[curr])
        pluck(curr)
    else:
        mand_inputs_1 = []
        mand_inputs_2 = []
        mand_outputs = []
        for curr in and_queue:
            mand_inputs_1.append(gate_specs[curr][2])
            mand_inputs_2.append(gate_specs[curr][3])
            mand_outputs.append(gate_specs[curr][4])
        new_gates.append([2 * len(and_queue), len(and_queue)] + mand_inputs_1 + 
                        mand_inputs_2 + mand_outputs + ["MAND"])
        and_queue_copy = and_queue.copy()
        and_queue.clear()
        for curr in and_queue_copy:
            pluck(curr)

print(len(new_gates), num_wire)
print(input_output_spec)
print()
for gate in new_gates:
    print(" ".join(map(str, gate)))
            
