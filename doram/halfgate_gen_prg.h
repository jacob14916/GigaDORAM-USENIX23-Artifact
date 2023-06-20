#pragma once

#include "emp-tool/utils/mitccrh.h"
#include "emp-tool/execution/circuit_execution.h"
#include "emp-tool/gc/halfgate_gen.h"
#include <iostream>
namespace emp {

// TODO: one of:
// a) Get the new constructors added to upstream emp-tool
// b) Inherit rather than copy-paste

template<typename T>
class HalfGateGenCustomPRG:public CircuitExecution {
public:
	block delta;
	T * io;
	block constant[2];
	MITCCRH<8> mitccrh;
	HalfGateGenCustomPRG(T * io) :io(io) {
		block tmp[2];
		PRG().random_block(tmp, 2);
		set_delta(tmp[0]);
		io->send_block(tmp+1, 1);
		mitccrh.setS(tmp[1]);
	}
	void set_delta(const block & _delta) {
		delta = set_bit(_delta, 0);
		PRG().random_block(constant, 2);
		io->send_block(constant, 2);
		constant[1] = constant[1] ^ delta;
	}
	// added these for replicated garbling
	HalfGateGenCustomPRG(T * io, PRG *prg) :io(io) {
		block tmp[2];
		prg->random_block(tmp, 2);
		set_delta(tmp[0], prg);
		io->send_block(tmp+1, 1);
		mitccrh.setS(tmp[1]);
	}
	void set_delta(const block & _delta, PRG *prg) {
		delta = set_bit(_delta, 0);
		prg->random_block(constant, 2);
		io->send_block(constant, 2);
		constant[1] = constant[1] ^ delta;
	}
	block public_label(bool b) override {
		return constant[b];
	}
	block and_gate(const block& a, const block& b) override {
		block table[2];
		block res = halfgates_garble(a, a^delta, b, b^delta, delta, table, &mitccrh);
		io->send_block(table, 2);
		return res;
	}
	block xor_gate(const block&a, const block& b) override {
		return a ^ b;
	}
	block not_gate(const block&a) override {
		return xor_gate(a, public_label(true));
	}
	uint64_t num_and() override {
		return mitccrh.gid/2;
	}
};
}
