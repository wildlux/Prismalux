#!/usr/bin/env python3
# =============================================================================
# ⚠️  STRUMENTO DIDATTICO — divide un carico di lavoro su N nodi (demo).
# Gli IP in deploy_shards.sh sono FITTIZI: adattali alla tua rete prima dell'uso.
# =============================================================================
"""
sharding_master.py - Divide il brute force su N nodi (es. 100)
"""

import json
import math
from typing import List


def split_workload(total_candidates: List[dict], num_nodes: int = 100):
    """Divide la lista di candidati in N pacchetti (shard)."""
    shard_size = math.ceil(len(total_candidates) / num_nodes)
    shards = []
    for i in range(num_nodes):
        start = i * shard_size
        end = min(start + shard_size, len(total_candidates))
        if start >= len(total_candidates):
            break
        shards.append({
            "node_id": i,
            "shard_size": end - start,
            "candidates": total_candidates[start:end]
        })
    return shards


if __name__ == "__main__":
    all_candidates = [{"seq": f"ATCG{str(i).zfill(4)}", "id": i} for i in range(700000)]
    shards = split_workload(all_candidates, num_nodes=100)

    for shard in shards:
        with open(f"shard_{shard['node_id']}.json", "w") as f:
            json.dump(shard, f)
        print(f"Shard {shard['node_id']}: {shard['shard_size']} candidati")

    with open("deploy_shards.sh", "w") as f:
        f.write("#!/bin/bash\n")
        f.write("# IP FITTIZI: adatta alla tua rete prima di eseguire.\n")
        for shard in shards:
            node_ip = f"192.168.1.{10 + shard['node_id']}"
            f.write(f"scp shard_{shard['node_id']}.json user@{node_ip}:/home/user/sds_worker/\n")
        f.write("echo 'Shard distribuiti a tutti i nodi.'\n")

    print("Sharding completato: shard_*.json e deploy_shards.sh")
