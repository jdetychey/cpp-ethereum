/*
	This file is part of cpp-ethereum.
	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file Executive.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include <functional>
#include <libdevcore/Log.h>
#include <libevmcore/Instruction.h>
#include <libethcore/Common.h>
#include <libevm/VMFace.h>
#include "Transaction.h"

namespace dev
{
namespace eth
{

class State;
class BlockChain;
class ExtVM;
struct Manifest;

struct VMTraceChannel: public LogChannel { static const char* name(); static const int verbosity = 11; };

/**
 * @brief Message-call/contract-creation executor; useful for executing transactions.
 *
 * Two ways of using this class - either as a transaction executive or a CALL/CREATE executive.
 *
 * In the first use, after construction, begin with initialize(), then execute() and end with finalize(). Call go()
 * after execute() only if it returns false.
 *
 * In the second use, after construction, begin with call() or create() and end with
 * accrueSubState(). Call go() after call()/create() only if it returns false.
 *
 * Example:
 * @code
 * Executive e(state, blockchain, 0);
 * e.initialize(transaction);
 * if (!e.execute())
 *    e.go();
 * e.finalize();
 * @endcode
 */
class Executive
{
public:
	/// Basic constructor.
	Executive(State& _s, LastHashes const& _lh, unsigned _level = 0): m_s(_s), m_lastHashes(_lh), m_depth(_level) {}
	/// Basic constructor.
	Executive(State& _s, BlockChain const& _bc, unsigned _level = 0);
	/// Basic destructor.
	~Executive() = default;

	Executive(Executive const&) = delete;
	void operator=(Executive) = delete;

	/// Initializes the executive for evaluating a transaction. You must call finalize() at some point following this.
	void initialize(bytesConstRef _transaction) { initialize(Transaction(_transaction, CheckTransaction::None)); }
	void initialize(Transaction const& _transaction);
	/// Finalise a transaction previously set up with initialize().
	/// @warning Only valid after initialize() and execute(), and possibly go().
	void finalize();
	/// Begins execution of a transaction. You must call finalize() following this.
	/// @returns true if the transaction is done, false if go() must be called.
	bool execute();
	/// @returns the transaction from initialize().
	/// @warning Only valid after initialize().
	Transaction const& t() const { return m_t; }
	/// @returns the log entries created by this operation.
	/// @warning Only valid after finalise().
	LogEntries const& logs() const { return m_logs; }
	/// @returns total gas used in the transaction/operation.
	/// @warning Only valid after finalise().
	u256 gasUsed() const;

	/// Set up the executive for evaluating a bare CREATE (contract-creation) operation.
	/// @returns false iff go() must be called (and thus a VM execution in required).
	bool create(Address _txSender, u256 _endowment, u256 _gasPrice, u256 _gas, bytesConstRef _code, Address _originAddress);
	/// Set up the executive for evaluating a bare CALL (message call) operation.
	/// @returns false iff go() must be called (and thus a VM execution in required).
	bool call(Address _receiveAddress, Address _txSender, u256 _txValue, u256 _gasPrice, bytesConstRef _txData, u256 _gas);
	bool call(CallParameters const& _cp, u256 const& _gasPrice, Address const& _origin);
	/// Finalise an operation through accruing the substate into the parent context.
	void accrueSubState(SubState& _parentContext);

	/// Executes (or continues execution of) the VM.
	/// @returns false iff go() must be called again to finish the transaction.
	bool go(OnOpFunc const& _onOp = OnOpFunc());

	/// Operation function for providing a simple trace of the VM execution.
	static OnOpFunc simpleTrace();

	/// @returns gas remaining after the transaction/operation. Valid after the transaction has been executed.
	u256 gas() const { return m_gas; }

	/// @returns the new address for the created contract in the CREATE operation.
	h160 newAddress() const { return m_newAddress; }
	/// @returns true iff the operation ended with a VM exception.
	bool excepted() const { return m_excepted != TransactionException::None; }

	/// Collect execution results in the result storage provided.
	void collectResult(ExecutionResult& _res) { m_res = &_res; }

private:
	State& m_s;							///< The state to which this operation/transaction is applied.
	LastHashes m_lastHashes;
	std::shared_ptr<ExtVM> m_ext;		///< The VM externality object for the VM execution or null if no VM is required.
	bytesRef m_outRef;					///< Reference to "expected output" buffer.
	ExecutionResult* m_res = nullptr;	///< Optional storage for execution results.
	Address m_newAddress;				///< The address of the created contract in the case of create() being called.

	unsigned m_depth = 0;				///< The context's call-depth.
	bool m_isCreation = false;			///< True if the transaction creates a contract, or if create() is called.
	TransactionException m_excepted = TransactionException::None;	///< Details if the VM's execution resulted in an exception.
	u256 m_gas = 0;						///< The gas for EVM code execution. Initial amount before go() execution, final amount after go() execution.

	Transaction m_t;					///< The original transaction. Set by setup().
	LogEntries m_logs;					///< The log entries created by this transaction. Set by finalize().

	bigint m_gasRequired;				///< Gas required during execution of the transaction.
	bigint m_gasCost;
	bigint m_totalCost;
};

}
}
