"""Compile and simulate a mixed-cut trace against the pinned CFS circuits."""
import argparse,json,random
from pathlib import Path
from cfs_benchmark import cfs,ScheduledTree,model_for,summarize,verify_values,verify_phases

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('contract',type=Path);p.add_argument('trace',type=Path);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();contract=json.loads(a.contract.read_text());candidate=json.loads(a.trace.read_text())
    label,prime=contract['curve'],contract['prime'];curve=cfs.curve_parameters(label)
    bits,rns_len,qm,u=cfs.rns_parameters(cfs.rns_bit_size(label));aa,b3=curve.ec.a%prime,3*curve.ec.b%prime
    leaves,lengths=cfs.leaf_circuits(label,prime,aa,b3);node=cfs.ECRNSPointAddition(prime,aa,b3);phase=cfs.ECRNSPointPhaseCorrection(prime,aa,b3)
    root=cfs.ECRNSRootPointAdditionSide(prime,aa,b3,curve.ec.q,qm+u,contract['qM_cofac'],contract['Mpwp_cofac'])
    qc=ScheduledTree(leaf_circuits=leaves,node_circuit=node,node_phase_correction=phase,root_circuit=root,schedule=tuple(map(tuple,candidate['trace'])))
    m=model_for(qc,node,phase,root);summary=summarize(qc,m)
    assert summary['work']==candidate['work'] and summary['peak_qubits']==candidate['peak_qubits']
    assert summary['peak_qubits']<=contract['cap']
    assert summary['compiled_qubits']<=contract['compiled_cap']
    native=ScheduledTree(leaf_circuits=leaves,node_circuit=node,node_phase_correction=phase,root_circuit=root)
    baseline=summarize(native,m)
    recorded=contract['rows'][contract['baseline']]
    for key in ('work','peak_qubits','compiled_qubits','counts'):
        assert baseline[key]==recorded[key], (key,baseline[key],recorded[key])
    basis=verify_values(qc,label,prime,lengths,4,contract['qM_cofac'],contract['Mpwp_cofac'])
    phases=verify_phases(qc,label,prime,lengths,2,contract['qM_cofac'],contract['Mpwp_cofac'])
    baseline_basis=verify_values(native,label,prime,lengths,4,contract['qM_cofac'],contract['Mpwp_cofac'])
    baseline_phases=verify_phases(native,label,prime,lengths,2,contract['qM_cofac'],contract['Mpwp_cofac'])
    result=dict(curve=label,prime=prime,qM_cofac=contract['qM_cofac'],
                Mpwp_cofac=contract['Mpwp_cofac'],
                summary={k:v for k,v in summary.items() if k!='trace'},
                baseline_summary={k:v for k,v in baseline.items() if k!='trace'},
                basis_trials=len(basis),phase_checks=phases,
                baseline_basis_trials=len(baseline_basis),baseline_phase_checks=baseline_phases)
    a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
