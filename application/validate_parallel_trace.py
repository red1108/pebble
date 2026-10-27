"""Compile and verify the direct manuscript schedule on the public CFS kernels."""
import argparse
import json
from pathlib import Path
from cfs_benchmark import cfs, ScheduledTree, model_for, summarize, verify_values, verify_phases
from verify_trace import verify

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('contract', type=Path)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
data = json.loads(args.contract.read_text())
label, prime = data['curve'], data['prime']
curve = cfs.curve_parameters(label)
_,_,qm,u = cfs.rns_parameters(cfs.rns_bit_size(label))
a,b3 = curve.ec.a % prime, 3*curve.ec.b % prime
leaves,lengths = cfs.leaf_circuits(label,prime,a,b3)
node = cfs.ECRNSPointAddition(prime,a,b3)
phase = cfs.ECRNSPointPhaseCorrection(prime,a,b3)
root = cfs.ECRNSRootPointAdditionSide(prime,a,b3,curve.ec.q,qm+u,
                                    data['qM_cofac'],data['Mpwp_cofac'])
saved = data['rows']['manuscript_parallel']
qc = ScheduledTree(leaf_circuits=leaves,node_circuit=node,node_phase_correction=phase,
                   root_circuit=root,schedule=tuple(map(tuple,saved['trace'])))
model = model_for(qc,node,phase,root)
row = summarize(qc,model)
checked = model.check_rounds(saved['rounds'],data['cap'])
assert checked['trace'] == list(map(tuple,saved['trace']))
assert {k:v for k,v in checked.items() if k != 'trace'} == saved['parallel']
for key in ('work','counts','peak_qubits','compiled_qubits'):
    assert row[key] == saved[key],key
for seed in range(32): verify(data,row,seed,compiled_cap=data['cap'])
basis = verify_values(qc,label,prime,lengths,4,data['qM_cofac'],data['Mpwp_cofac'])
phases = verify_phases(qc,label,prime,lengths,2,data['qM_cofac'],data['Mpwp_cofac'])
result = dict(summary={k:v for k,v in row.items() if k != 'trace'},
              parallel=saved['parallel'],adaptation=saved['adaptation'],
              basis_trials=len(basis),phase_checks=phases,independent_parity_trials=32)
args.output.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
