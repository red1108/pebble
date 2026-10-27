"""Lower the shared manuscript schedule to the fixed CFS circuit tree."""
from shared_scheduler import parallel_rounds


def schedule(model, cap):
    labels = {}
    def embed(v, x):
        labels[x] = v
        for i, child in enumerate(model.children[v]):
            embed(child, 2*x+i)
    embed(model.root, 1)
    height = max(x.bit_length() for x in labels)
    scratch = max(model.cost(op, v).scratch
                  for v in model.children if v != model.root for op in ('P', 'U'))
    space = min((cap-model.fixed-scratch)//model.width, (1<<height)-1)
    if space < height+1:
        raise ValueError('The CFS cap cannot hold the manuscript schedule and one kernel scratch area')
    raw = parallel_rounds(height, space)

    # Missing descendants of shallow leaves are virtual and have no circuit calls.
    phases = []
    for pebble, unpebble, measure in raw:
        ops = [('R' if labels[x] == model.root else 'P', labels[x])
               for x in pebble if x in labels]
        ops += [('U', labels[x]) for x in unpebble if x in labels]
        phases.append(ops)
        phases.append([('G', labels[x]) for x in measure
                       if x in labels and labels[x] not in model.leaves])
        # The public lookup circuit clears leaves by its inverse, not by ghosting.
        phases.append([('U', labels[x]) for x in measure
                       if x in labels and labels[x] in model.leaves])

    # Lower an unused ghost recomputation followed by erasure to direct correction.
    flat = [op for phase in phases for op in phase]
    touched = {}
    next_use = {}
    for i in range(len(flat)-1, -1, -1):
        op, v = flat[i]
        next_use[i] = touched.get(v)
        for x in [v] + (list(model.children[v]) if op != 'G' else []):
            touched[x] = i
    state = dict.fromkeys(model.children, 0)
    skip, correct = set(), set()
    empty_leaves = 0
    for i, (op, v) in enumerate(flat):
        if op == 'P':
            j = next_use[i]
            if j is not None and flat[j] == ('U', v):
                if state[v] == 2:
                    correct.add(i)
                    skip.add(j)
                elif v in model.leaves:
                    # A shallow leaf has already been erased; its virtual
                    # descendants create no remaining cleanup obligation.
                    skip.update((i,j))
                    empty_leaves += 1
            state[v] = 1
        elif op == 'R': state[v] = 1
        elif op == 'G': state[v] = 2
        else: state[v] = 0

    state = dict.fromkeys(model.children, 0)
    rounds = []
    def occupied(ops):
        live = sum(s == 1 for v, s in state.items() if v != model.root)
        new = sum(op == 'P' for op, v in ops)
        return model.fixed + (live+new)*model.width + sum(model.cost(op,v).scratch for op,v in ops)
    def emit(ops):
        if not ops: return
        rounds.append(dict(ops=list(ops), measure=[]))
        for op,v in ops:
            state[v] = 1 if op in ('P','R') else 0
    i = 0
    for phase in phases:
        lowered = []
        for op,v in phase:
            if i not in skip: lowered.append(('C' if i in correct else op,v))
            i += 1
        if not lowered: continue
        if lowered[0][0] == 'G':
            assert rounds and all(op == 'G' for op,v in lowered)
            rounds[-1]['measure'].extend(v for op,v in lowered)
            for op,v in lowered: state[v] = 2
            continue
        batch = []
        for op,v in lowered:
            if occupied(batch+[(op,v)]) > cap:
                emit(batch)
                batch = []
            if occupied([(op,v)]) > cap:
                raise ValueError(('Kernel exceeds the shared cap',op,v,occupied([(op,v)]),cap))
            batch.append((op,v))
        emit(batch)
    return rounds, dict(embedding_height=height, logical_space=space,
                        reserved_kernel_scratch=scratch, abstract_rounds=len(raw),
                        virtual_vertices=(1<<height)-1-len(labels),
                        direct_corrections=len(correct), empty_leaf_pairs=empty_leaves)
